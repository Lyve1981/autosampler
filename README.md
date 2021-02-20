# Autosampler
Autosampler can create multisamples of hardware MIDI devices.
It opens a MIDI port to send notes and an Audio input to record audio data.

Audio data is automatically cut & trimmed and written to folders according to
the 'filename' scheme (see below).

Usage:

All arguments need to be specified in form -arg value

Possible arguments:

    ai-device             Specify the audio device to be used to capture data. Can be
                          empty in which case the default device is used
                          Example: Windows DirectSound
    
    ai-api                Specify the audio host API to be used. Can be empty in which
                          case the default api is used.
                          Example: Input (High Definition Audio Device)
    
    ai-bitrate            Specify the bit depth at which audio is recorded.
                          Default: 24
                          Examples: 16 / 24 / 32
    
    ai-samplerate         Specify the sample rate at which audio is recorded.
                          Default: 48000
                          Examples: 44100 / 48000 / 96000
    
    ai-channels           Specify the number of input channels that are recorded. Default
                          mono = 1, stereo would be 2
                          Default: 1
                          Examples: 1 / 2 / 6 / 8
    
    ai-blocksize          Specify the block size at which audio is processed.
                          Default: 1024
                          Examples: 512 / 1024 / 2048
    
    mo-device             Specify the MIDI device to be used to send midi data. Can be
                          empty in which case the default device is used
                          Example: MIDIOUT2 (BCR2000)
    
    mo-api                Specify the MIDI host API to be used. Can be empty in which
                          case the default api is used. On some systems, for example
                          on Windows, there is only one API anyway.
                          Example: MMSystem
    
    midi-notes            Specify the MIDI notes to be played. Can be specified as a
                          single note
                          Default: 0-127
                          Examples: 60 / 0-127 / 30,60,90
    
    midi-velocities       Specify the velocities for note on events.
                          Default: 127
                          Examples: 60 / 0-127 / 30,60,90
    
    midi-programs         A list of program changes that are sent to the device
                          Examples: 60 / 0-127 / 30,60,90
    
    pause-before          Pause time in seconds before the next note is being recorded.
                          During this time, program changes are sent, if applicable
                          Default: 0.5
                          Example: 1.0
    
    pause-after           Additional pause time in seconds after release has finished.
                          Default: 0.5
                          Example: 1.0
    
    sustain-time          Specify how many seconds a note is held down before released.
                          Default: 3
                          Example: 3.5
    
    release-time          Specify how many seconds recording is continued after a note
                          has been released.
                          Default: 1
                          Example: 3.5
    
    release-velocity      Release velocity that is sent to the device when a note is
                          released.
                          Default:
                          Example: 3.5
    
    midi-channel          The MIDI channel that events are sent on. Range 0-15
                          Default:
                          Examples: 0 / 15
    
    noisefloor-duration   Noise floor is detected after program start, used to trim
                          wave files to remove silence before/after the recording of
                          a note. Specify the duration of noise floor detected here.
                          Default: 2
                          Examples: 3.0 / 5
    
    filename              Specify the filename that is used to create a recording. Some
                          variables can be used to customize the file name and the path:
    
                          {note} Note number in range 0-127
    
                          {key} Note a human readable string like C#4. F#3, range is
                          C-2 to G8
    
                          {velocity} Velocity in range 0-127
    
                          {program} Program change in range 0-127
                          Example: ~/autosampler/device/patch{program}/{note}_{key}_{velocity}.wav
    
