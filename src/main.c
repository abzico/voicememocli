#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define PRINT(x, ...) fprintf(stdout, x, ##__VA_ARGS__)
#define PERROR(x, ...) fprintf(stderr, x, ##__VA_ARGS__)

enum CmdType
{
  CMDTYPE_SHOW,
  CMDTYPE_LISTEN,
  CMDTYPE_RECORD,
  CMDTYPE_HELP
};

struct ArgCmdRecord
{
  // limit to maximum of 4 characters + 1 for null-terminated character of key
  // via -k|--key
  char key[5];
  // recording device
  // via -d|--device
  int device;
};

// -- function prototypes -- //
static bool init();
static void close();
static void print_all_available_device_names();
static void print_help();
static void update(float deltaTime);
static void handle_command(enum CmdType cmd_type, void* data);

// recording/playback callbacks
static void audio_recording_callback(void* userdata, Uint8* stream, int len);
static void audio_playback_callback(void* userdata, Uint8* stream, int len);
// -- end of function prorotypes --//

// max number of supported recording devices
const int MAX_RECORDING_DEVICES = 5;
// max recording time
const int MAX_RECORDING_SECONDS = 5;
// max recording time plus padding
const int RECORDING_BUFFER_SECONDS = MAX_RECORDING_SECONDS + 1;

// various state of recording actions
enum RecordingState
{
  SELECTING_DEVICE,
  STOPPED,
  RECORDING,
  RECORDED,
  PLAYBACK,
  ERROR
};

SDL_Color text_color = {0,0,0,0xff};

// available device names
// this will be pointer to array of char*
char** available_device_names = NULL;
int available_recording_device_count = 0;

// received audio spec
SDL_AudioSpec received_recording_spec;
SDL_AudioSpec received_playback_spec;

// recording data buffer
Uint8* recording_buffer = NULL;
// size of data buffer
Uint32 buffer_byte_size = 0;
// position in data buffer
Uint32 buffer_byte_position = 0;
// maximum position in data buffer for recording
Uint32 buffer_byte_max_position = 0;

// recording state
enum RecordingState recording_state = SELECTING_DEVICE;
// audio device IDs
SDL_AudioDeviceID recording_deviceid = 0;
SDL_AudioDeviceID playback_deviceid = 0;

bool init()
{
  // get capture device count
  // note: pass positive number to get recording device, pass negative or zero to get playback device
  available_recording_device_count = SDL_GetNumAudioDevices(SDL_TRUE);
  if (available_recording_device_count < 1)
  {
    PERROR("[Error] Unable to get audio capture device! %s\n", SDL_GetError());
    // return now as we can't proceed with this sample app
    return false;
  }
  else
  {
    // cap recording device count
    if (available_recording_device_count > MAX_RECORDING_DEVICES)
    {
      available_recording_device_count = MAX_RECORDING_DEVICES;
    }

    // allocate enough memory space of array to hold all available device names
    available_device_names = malloc(sizeof(char*) * available_recording_device_count);

    // generate texture for recording devices' name
    char temp_chrs[120];
    for (int i=0; i<available_recording_device_count; i++)
    {
      // get name of device
      // specify SDL_TRUE to get name of recording device
      const char* device_name = SDL_GetAudioDeviceName(i, SDL_TRUE);

      if (device_name != NULL)
      {
        // form string to show on screen
        snprintf(temp_chrs, sizeof(temp_chrs), "[%d] : %s", i, device_name);
        
        // allocate new memory space for string
        available_device_names[i] = calloc(1, sizeof(char) * strlen(temp_chrs) + 1);
        // copy string for us to use it later
        strncpy(available_device_names[i], temp_chrs, sizeof(char) * strlen(temp_chrs));
      }
    }
  }

  return true;
}

void close()
{
  // free holding list of available device names
  if (available_device_names != NULL)
  {
    for (int i=0; i<available_recording_device_count; i++)
    {
      if (available_device_names[i] != NULL)
      {
        free(available_device_names[i]);
        available_device_names[i] = NULL;
      }
    }

    // free the source of holder
    free(available_device_names);
    available_device_names = NULL;
  }

  SDL_Quit();
}

void print_all_available_device_names()
{
  for (int i=0; i<available_recording_device_count; i++)
  {
    PRINT("%s\n", available_device_names[i]);
  }
}

void print_help()
{
  PRINT("Available commands\n");
  PRINT(" show - show all recording devices along with its indexes\n");
  PRINT(" listen <KEY> - listen to voice memo of specified key\n");
  PRINT(" record <KEY> - record voice memo with specified key\n");
  PRINT(" help - show this info\n");
}

void audio_recording_callback(void* userdata, Uint8* stream, int len)
{
  // copy audio from stream
  memcpy(&recording_buffer[buffer_byte_position], stream, len);

  // move along buffer
  buffer_byte_position += len;
}

void audio_playback_callback(void* userdata, Uint8* stream, int len)
{
  // copy audio to stream
  memcpy(stream, &recording_buffer[buffer_byte_position], len);

  // move along buffer
  buffer_byte_position += len;
}

void update(float deltaTime)
{
  // checking to finish the recording process after it has been set to record
  if (recording_state == RECORDING)
  {
    // if recorded passed 5 seconds mark
    if (buffer_byte_position > buffer_byte_max_position)
    {
      // lock callback (different thread)
      SDL_LockAudioDevice(recording_deviceid);

      // stop recording audio
      SDL_PauseAudioDevice(recording_deviceid, SDL_TRUE);

      // print current state
      PRINT("Recorded successfully\n");
      recording_state = RECORDED;

      // unlock callback
      SDL_UnlockAudioDevice(recording_deviceid);
    }
  }
  else if (recording_state == PLAYBACK)
  {
    // check if finish playback
    if (buffer_byte_position > buffer_byte_max_position)
    {
      // lock callback (different thread)
      SDL_LockAudioDevice(playback_deviceid);

      // stop playback
      SDL_PauseAudioDevice(playback_deviceid, SDL_TRUE);

      // print current state
      PRINT("Playback complete\n");
      recording_state = RECORDED;

      // unlock callback
      SDL_UnlockAudioDevice(playback_deviceid);
    }
  }
}

void handle_command(enum CmdType cmd_type, void* data)
{
  if (cmd_type == CMDTYPE_SHOW)
  {
    print_all_available_device_names();
  }
  else if (cmd_type == CMDTYPE_HELP)
  {
    print_help();
  }
  else if (cmd_type == CMDTYPE_LISTEN)
  {
    // convert parameter into string
    char* key = (char*)data;
    PRINT("key = %s\n", key);
  }
  else if (cmd_type == CMDTYPE_RECORD)
  {
    // convert to ArgCmdRecord
    struct ArgCmdRecord param = *(struct ArgCmdRecord*)data;
    PRINT(": param key = %s\n", param.key);
    PRINT(": param device = %d\n", param.device);

    // 2 things need to do next
    // 1. open device
    // 2. immediately start recording

    // index is valid
    if (param.device < available_recording_device_count)
    {
      // 1. OPEN DEVICE
      // default audio spec
      SDL_AudioSpec desired_recording_spec;
      SDL_zero(desired_recording_spec);
      desired_recording_spec.freq = 44100;
      desired_recording_spec.format = AUDIO_F32;
      desired_recording_spec.channels = 2;
      desired_recording_spec.samples = 4096;
      desired_recording_spec.callback = audio_recording_callback;

      // open recording device
      recording_deviceid = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(param.device, SDL_TRUE), SDL_TRUE, &desired_recording_spec, &received_recording_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
      // if error
      if (recording_deviceid == 0)
      {
        // update current status
        PERROR("[Error] Failed to open audio device [%d]: %s\n", param.device, SDL_GetError());
        recording_state = ERROR;
      }
      // device opens successfully
      else
      {
        // default audio spec
        SDL_AudioSpec desired_playback_spec;
        SDL_zero(desired_playback_spec);
        desired_playback_spec.freq = 44100;
        desired_playback_spec.format = AUDIO_F32;
        desired_playback_spec.channels = 2;
        desired_playback_spec.samples = 4096;
        desired_playback_spec.callback = audio_playback_callback;

        // open playback device
        // note: pass NULL to indicate that we don't care which playback device we'd got, just get the first available one
        playback_deviceid = SDL_OpenAudioDevice(NULL, SDL_FALSE, &desired_playback_spec, &received_playback_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        // error
        if (playback_deviceid == 0)
        {
          // update current status
          PERROR("[Error] Failed to open playback device! %s\n", SDL_GetError());
          recording_state = ERROR;
        }
        // device playback opens successfully
        else
        {
          // calculate per sample bytes
          int bytes_per_sample = received_recording_spec.channels * (SDL_AUDIO_BITSIZE(received_recording_spec.format) / 8);

          // calculate bytes per second
          int bytes_per_second = received_recording_spec.freq * bytes_per_sample;

          // calculate buffer size
          buffer_byte_size = RECORDING_BUFFER_SECONDS * bytes_per_second;

          // calculate max buffer use
          buffer_byte_max_position = MAX_RECORDING_SECONDS * bytes_per_second;

          // allocate and initialize byte buffer
          recording_buffer = calloc(1, buffer_byte_size);

          PRINT("buffer size = %d\n", buffer_byte_size);

          // now it's in STOPPED state ready for recording!
          recording_state = STOPPED;
        
          // update status
          PRINT("Device opened successfully\n");

          // 2. RECORDING
          // go back to the beginning of buffer
          buffer_byte_position = 0;

          // start recording
          SDL_PauseAudioDevice(recording_deviceid, SDL_FALSE);
          
          // print current state
          PRINT("Recording...\n");
          recording_state = RECORDING;
        }
      }
    }
  }

  char key_press = 'd';

  switch (recording_state)
  {
    case RECORDED:
      // press 1 to play back
      if (key_press == '1')
      {
        // go back to beginning of buffer
        buffer_byte_position = 0;

        // start playback
        SDL_PauseAudioDevice(playback_deviceid, SDL_FALSE);

        // print current state
        PRINT("Playing...\n");
        recording_state = PLAYBACK;
      }
      // press 2 to re-record again
      else if (key_press == '2')
      {
        // reset buffer
        buffer_byte_position = 0;
        memset(recording_buffer, 0, buffer_byte_size);

        // start recording
        SDL_PauseAudioDevice(recording_deviceid, SDL_FALSE);

        // print current state
        PRINT("Recording...\n");
        recording_state = RECORDING;
      }
      break;
  }
}

int main(int argc, char* argv[])
{
  // initialization without video system
  putenv("SDL_VIDEODRIVER=dummy");

  // we still need to specify SDL_INIT_VIDEO
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    PERROR("[Error] Failed to init: %s\n", SDL_GetError());
    return -1;
  }

  if (!init())
  {
    PERROR("[Error] Failed to init\n");
    goto CLOSE;
  }

  // check the command
  if (argc < 2)
  {
    PERROR("Usage voicememo <command> [options]\n");
    PERROR("Try voicememo help\n");
    close();

    return -1;
  }

  // otherwise at least has enough argument to be processed
  if (strncmp(argv[1], "show", 4) == 0)
  {
    handle_command(CMDTYPE_SHOW, NULL);
  }
  else if (strncmp(argv[1], "listen", 6) == 0)
  {
    // check whether it has enough number of required arguments
    if (argc < 3)
    {
      PERROR("Not enough arguments\n");
      PERROR("Usage voicememo listen <KEY>\n");
      close();
      return -1;
    }
    else
    {
      // send in KEY as string but converted to opaque pointer
      handle_command(CMDTYPE_LISTEN, (void*)argv[2]);
    }
  }
  else if (strncmp(argv[1], "record", 6) == 0)
  {
    // check whether it has enough number of required arguments
    if (argc < 6)
    {
RECORD_PRINT_ERROR:
      PERROR("Not enough arguments\n");
      PERROR("Usage voicememo record -k|--key <KEY> -d|--device <INDEX>\n");
      PERROR("\n");
      PERROR("Record voice memo with KEY by using INDEX recording device\n");
      PERROR("KEY will only be accepted up to 4 characters\n");
      close();
      return -1;
    }

    // struct of argument we gonna send to handle
    struct ArgCmdRecord arg_cmd_record;
    // zero out bytes of string
    memset(arg_cmd_record.key, 0, sizeof(arg_cmd_record.key));

    // result value of 3 indicates we found both of required parameters
    int found_bitchecks = 0;

    // look for -k|--key
    // checking starts from index 2 to lastindex-1
    for (int i=2; i<5; ++i)
    {
      if (strncmp(argv[i], "-k", 2) == 0 ||
          strncmp(argv[i], "--key", 5) == 0)
      {
        strncpy(arg_cmd_record.key, argv[i+1], sizeof(arg_cmd_record.key) - 1);
        found_bitchecks |= 0x1;
      }
      else if (strncmp(argv[i], "-d", 2) == 0 ||
          strncmp(argv[i], "--device", 8) == 0)
      {
        // in case of error, it returns 0 in which will default our
        // selection of device index to first one
        arg_cmd_record.device = strtol(argv[i+1], NULL, 10);
        found_bitchecks |= 0x2;
      }
    }

    // if user doesn't have all required parameters
    if ((found_bitchecks & 0x3) != 0x3)
    {
      goto RECORD_PRINT_ERROR;
    }
    // otherwise, user has all
    else
    {
      // send in KEY as string but converted to opaque pointer
      handle_command(CMDTYPE_RECORD, (void*)&arg_cmd_record);

      // if things went well, we should be recording by now
      if (recording_state == RECORDING)
      {
        Uint32 prevTime = 0, currTime = 0;

        // enter the recording loop
        do
        {
          prevTime = currTime;
          currTime = SDL_GetTicks();
          float delta_time = (currTime - prevTime) / 1000.0f;
          // update recording loop
          update(delta_time);
        } while (recording_state != RECORDED);

        // test playback
        // go back to beginning of buffer
        buffer_byte_position = 0;

        // start playback
        SDL_PauseAudioDevice(playback_deviceid, SDL_FALSE);

        // print current state
        PRINT("Playing...\n");
        recording_state = PLAYBACK;
        do
        {
          prevTime = currTime;
          currTime = SDL_GetTicks();
          float delta_time = (currTime - prevTime) / 1000.0f;
          // update recording loop
          update(delta_time);
        } while (recording_state != RECORDED);
      }
      // otherwise, error, along the way we printed error message already
      // so we just quit
      else
      {
        close();
        return -1;
      }
    }
  }
  else if (strncmp(argv[1], "help", 4) == 0)
  {
    handle_command(CMDTYPE_HELP, NULL);
  }
  else
  {
    PERROR("[Error] Unrecognized command\n");
    close();
    return -1;
  }

CLOSE:
  close();

  return 0;
}
