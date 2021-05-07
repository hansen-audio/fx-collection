# FX Colletion Library

## Motivation

This library combines basic classes from dsp-tool-box to audio effects. The trance gate effect for example uses three modulation phases and a one pole filter from the dsp-tool-box. 

## Building the project

Execute the following commands on cli.

```
git clone https://www.github.com/hansen-audio/fx-collection.git
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ../fx-collection
cmake --build .
```

### CMake Generators

CMake geneartors for all platforms.

* Linux: ```cmake -DCMAKE_BUILD_TYPE=[Debug|Release] ...```
* macOS: ```cmake -GXcode ...```
* Windows 10: ```cmake -G"Visual Studio 16 2019" -A x64 ...```

## Using the modules

All module classes in this library contain a ```context``` and ```static``` methods in order to modify the ```context```. Like this the data and the algorithm are separated and allow a usage in a multithreaded environment.

### Setting parameters of the context

Use the ```trance_gate::create``` method in order to get a valid ```context```. Afterwards use the ```static``` methods like ```set_mix``` to modify the ```context```.

```
auto tg_context = ha::fx_collection::trance_gate::create();
ha::fx_collection::trance_gate::set_mix(tg_context, 1.);
```

### Process the trance gate

In order to process the trance gate, use the ```process``` method. This method takes an input ```audio_frame``` and an output ```audio_frame```. The ```audio_frame``` struct contains an alinged buffer of four channels.

```
auto tg_context = ha::fx_collection::trance_gate::create();
ha::fx_collection::trance_gate::set_mix(tg_context, 1.);

ha::fx_collection::audio_frame input; // Insert your sample here
ha::fx_collection::audio_frame output;

ha::fx_collection::trance_gate::process(tg_context, input, output);

// Use the output for further processing
```

## License

Copyright 2021 Hansen Audio

Licensed under the MIT: https://mit-license.org/
