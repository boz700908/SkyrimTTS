# SkyrimTTS

An SKSE plugin that makes Skyrim AE UI accessible to blind and visually impaired players.
- Uses [SRAL](https://github.com/m1maker/SRAL) (Screen Reader Abstraction Library) to provide screen reader support through NVDA, JAWS, or SAPI.  
- Uses [CommonLibSSE](https://github.com/libxse/commonlibsse) (Address Library for SKSE Plugins) RE C++ SKSE to develop plugins.

### Requirements
* [XMake](https://xmake.io) [3.0.7+]

## Getting Started
```bash
git clone --recurse-submodules -b rewrite https://github.com/DioKyrie-Git/SkyrimTTS.git
cd SkyrimTTS
```

### Build
To build the project, run the following command:
```bash
xmake build
```

> ***Note:*** *This will generate a `build/windows/x64/release/` directory in the **project's root directory** with the build output.*
