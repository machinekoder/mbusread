//------------------------------------------------------------------------------
// Copyright (C) 2012, Alexander Rössler
// All rights reserved.
//
//
//------------------------------------------------------------------------------

#include "mbusread.h"

int
main(int argc, char *argv[])
{
    char* fileName = NULL;
    char c;
    uint8_t params = 0; //initialize application params
    
    while ((c = getopt(argc, argv, "f:x:")) != -1)
    {
        if (c == 'f')
        {
            fileName = optarg;
            params |= (1 << PARAM_F);
        }
        else if (c == 'x')
        {
            fileName = optarg;
            params |= (1 << PARAM_X);
        }
    }
	
    if (params & (1 << PARAM_F))        //process config file
    {
        if (fileName != NULL)
            process_config(fileName);
        else
            log_error("No config file given", __PRETTY_FUNCTION__);
    }
    
    if (params & (1 << PARAM_X))        //process hexdump
    {
        if (fileName != NULL)
            process_hexdump(fileName);
        else
            log_error("No hex file given", __PRETTY_FUNCTION__);
    }

    if ((argc == 1) || (params & (1 << PARAM_H)))
    {
        fprintf(stdout, "MBusRead\n"
                "usage: %s -f <config file name> -x <hexdump file name> -h <show this help>", argv[0]);
    }
}

mbus_handle*
connect_device(char* device, uint baudrate)
{
  mbus_handle *handle;
  if ((handle = mbus_connect_serial(device)) == NULL)
  {
    log_error("Failed to setup connection to M-bus gateway", __PRETTY_FUNCTION__);
    return NULL;
  }
  else
  {
    if (mbus_serial_set_baudrate(handle->m_serial_handle, baudrate) == -1)
    {
      log_error("Failed to set baud rate", __PRETTY_FUNCTION__);
      return NULL;
    }
  }
  
  return handle;
}

int 
request_primary(mbus_handle* handle, uint address)
{
  if (mbus_send_request_frame(handle, address) == -1)
  {
      char error_buf[100];
      snprintf(error_buf, sizeof(error_buf), "Failed to send M-Bus request frame: address: [%u]", address);
      log_error(error_buf, __PRETTY_FUNCTION__);
      return 0;
  }
  return 1;
}

int 
request_secondary(mbus_handle* handle, uint address)
{
  int probe_ret;
  char addr_str[16];
  char matching_addr[16];
  
  sprintf(addr_str, "%u", address);

  probe_ret = mbus_probe_secondary_address(handle, addr_str, matching_addr);

  if (probe_ret == MBUS_PROBE_COLLISION)
  {
      char error_buf[100];
      snprintf(error_buf, sizeof(error_buf), "The address mask [%s] matches more than one device.", addr_str);
      log_error(error_buf, __PRETTY_FUNCTION__);
      return 0;
  }
  else if (probe_ret == MBUS_PROBE_NOTHING)
  {
      char error_buf[100];
      snprintf(error_buf, sizeof(error_buf), "The selected secondary address does not match any device [%s].", addr_str);
      log_error(error_buf, __PRETTY_FUNCTION__);
      return 0;
  }
  else if (probe_ret == MBUS_PROBE_ERROR)
  {
      char error_buf[100];
      snprintf(error_buf, sizeof(error_buf), "Error: Failed to probe secondary address [%s].", addr_str);
      log_error(error_buf, __PRETTY_FUNCTION__);
      return 0;    
  }
  // else MBUS_PROBE_SINGLE

  if (mbus_send_request_frame(handle, 253) == -1)
  {
      char error_buf[100];
      snprintf(error_buf, sizeof(error_buf), "Failed to send M-Bus request frame: address: [%u]", address);
      log_error(error_buf, __PRETTY_FUNCTION__);
      return 0;
  }
  return 1;
}

int 
receive_frame(mbus_handle* handle, FILE *file, uint address)
{
  mbus_frame reply;
  mbus_frame_data reply_data;
  
  if (mbus_recv_frame(handle, &reply) == -1)
  {
    char error_buf[100];
    snprintf(error_buf, sizeof(error_buf), "Failed to receive M-Bus response frame. address: [%u]", address);
    log_error(error_buf, __PRETTY_FUNCTION__);
    return 0;
  }
  
  /*if (debug)
  {
    mbus_frame_print(&reply);
  }*/
  mbus_frame_data_parse(&reply, &reply_data);
  if (address > 255)
    address = 253;
  fprintf(file, "%s", mbus_frame_data_csv(&reply_data, address));
  
  // manual free
  if (reply_data.data_var.record)
  {
      mbus_data_record_free(reply_data.data_var.record); // free's up the whole list
  }
}

int 
compress_file(char* filename, char* zip_filename)
{
  char command[200];
  char error_filename[100];
  FILE *errorfile;
  
  errorfile = fopen("error.log", "r");	//test open log file
  if (errorfile != NULL)
    strcpy(error_filename, "error.log");
  else
    strcpy(error_filename, "");
  
  snprintf(command, sizeof(command),"zip %s %s %s", zip_filename, filename, error_filename);
  return system(command);
}

char* 
generate_filename()
{
  static char filename[100];
  time_t currenttime;
  time (&currenttime);
  struct tm *structured_time = localtime(&currenttime);
  snprintf(filename, sizeof(filename), "t%04d%02d%02d%02d", structured_time->tm_year + 1900,
							    structured_time->tm_mon+1, 
							    structured_time->tm_mday, 
							    structured_time->tm_hour);//date time
  //...
  return filename;
}

void 
process_config(char* config_filename)
{
  FILE *configfile;
  FILE *outputfile;
  
  char *output_filename = generate_filename();
  char tmp_filename[100];
  char text_filename[100];
  snprintf(tmp_filename, sizeof(tmp_filename), "%s.temp.txt", output_filename);
  snprintf(text_filename, sizeof(text_filename), "%s.txt", output_filename);
  
  configfile = fopen(config_filename, "r");  
  outputfile = fopen(tmp_filename, "a");
  
  if (outputfile == NULL)
  {
    if (configfile != NULL)
      fclose(configfile);
    
    log_error("Failed to open output file", __PRETTY_FUNCTION__);
    exit(EXIT_FAILURE);
  }
  
  if (configfile != NULL)
  {
    mbus_handle *handle = NULL;
    char line[100];
    char type[100];
    char device[100];
    uint baudrate = 2400;
    uint address;
    uint timeout;
    uint tb;
    int n=0;
    
    while (fgets(line, sizeof(line), configfile) != NULL)
    {
      if (n == 0)
      {
	sscanf(line,"%s", line);
	strcpy(device,"");
	
	if (strcmp(line, "COM1") == 0)
	  strcpy(device, "/dev/ttyS0");
	else if (strcmp(line, "COM2") == 0)
	  strcpy(device, "/dev/ttyS1");
	else if (strcmp(line, "COM3") == 0)
	  strcpy(device, "/dev/ttyS2");
	else if (strcmp(line, "COM4") == 0)
	  strcpy(device, "/dev/ttyS3");
	
	if ((handle = connect_device(device, baudrate)) == NULL)
	  exit(EXIT_FAILURE);
      }
      else if (n == 1)
      {
    sscanf(line, "TO %u", &timeout);
    mbus_serial_set_timeout(handle,timeout);
      }
      else if (n == 2) //these values are unused yet
      {
	sscanf(line, "TB %u", &tb);
      }
      else if (line[0] == '#')
	continue;
      else if (line[0] == 'p')	//use primary addressing
      {
	sscanf(line, "p %u TYP=%s", &address, type);
	if (request_primary(handle, address))
	  receive_frame(handle, outputfile, address);
      }
      else if (line[0] == 's')	//use secondary addressing
      {
	sscanf(line, "s %u TYP=%s", &address, type);
	if (request_secondary(handle, address))
	  receive_frame(handle, outputfile, address);
      }
      else
	continue;
      
      n++;
    }
    
    //disconnect mbus
    if (handle != NULL)
      mbus_disconnect(handle);
    //close files
    fclose(configfile);
    fclose(outputfile);
    //rename temporary file
    //rename(tmp_filename, text_filename);
    join_files(tmp_filename, text_filename);
    //compress file
    compress_file(text_filename, output_filename);
    //delete temporary files
    remove(tmp_filename);
  }
  else
  {
    log_error("Failed to open config file", __PRETTY_FUNCTION__);
    exit(EXIT_FAILURE);
  }
}

void process_hexdump (char* hexdumpfile_name)   //from libmbus
{
    int fd, len, i;
    u_char raw_buff[4096], buff[4096], *ptr, *endptr;
    mbus_frame reply;
    mbus_frame_data frame_data;

    if ((fd = open(hexdumpfile_name, O_RDONLY, 0)) == -1)
    {
        log_error("Failed to open hexdump file", __PRETTY_FUNCTION__);
        return;
    }

    //bzero(raw_buff, sizeof(raw_buff));
    memset(raw_buff,0, sizeof(raw_buff));

    len = read(fd, raw_buff, sizeof(raw_buff));
    close(fd);

    i = 0;
    ptr    = 0;
    endptr = raw_buff;
    while (i < sizeof(buff)-1)
    {
        ptr = endptr;
        buff[i] = (u_char)strtol(ptr, (char **)&endptr, 16);
        
        // abort at non hex value 
        if (ptr == endptr)
            break;
           
        i++;
    }

    //bzero(&reply, sizeof(reply));;
    memset(&reply,0, sizeof(reply));
    //bzero(&frame_data, sizeof(frame_data));
    memset(&frame_data,0, sizeof(frame_data));

    mbus_parse(&reply, buff, i);
    
    mbus_frame_data_parse(&reply, &frame_data);
    //mbus_frame_print(&reply);
    //mbus_frame_data_print(&frame_data);
    printf("%s", mbus_frame_data_csv(&frame_data, 253));
}

void 
log_error(char* error_msg, const char* function_name)
{
  FILE *logfile;
  
  
  logfile = fopen("error.log", "a");
  
  if (logfile != NULL)
  {
    time_t currenttime;
    struct tm *structured_time;
    char timebuf[100];
    
    time (&currenttime);
    structured_time = localtime(&currenttime);
    snprintf(timebuf, sizeof(timebuf), "%02d.%02d.%04d %02d:%02d:%02d", structured_time->tm_mday, 
									    structured_time->tm_mon+1, 
									    structured_time->tm_year + 1900,
									    structured_time->tm_hour,
									    structured_time->tm_min,
									    structured_time->tm_sec);
    fprintf(logfile, "%s: %s: Error: %s\n", timebuf, function_name, error_msg);
    fprintf(stderr, "%s: Error: %s\n", function_name, error_msg);
    
    fclose(logfile);
  }
}

int join_files(char* old_filename, char* new_filename)
{
  char command[200];
  
  snprintf(command, sizeof(command),"cat %s >> %s", old_filename, new_filename);
  return system(command);
}
