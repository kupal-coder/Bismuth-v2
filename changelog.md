# Unreleased
- Android support (32/64 bit) for devices with OpenGL ES 3.1
- Shaders are compiled as GLSL ES 3.10 on mobile instead of being preprocessed with glslang
- Automatic fallback to the game's renderer when the GPU can't run Bismuth
- New "Enable renderer on mobile" setting
- Buffer mapping now uses glMapBufferRange (works on ES, with a CPU fallback)
- Indirect draws work without multi-draw support (one draw per work group)
- Fixed overdraw visualizer / GPU timings crashing on drivers without the required features

# 0.1.0-alpha
- Added rotate trigger support
- Added follow trigger support
- Added shader sprite optimization
- Added seperate batch node optimization
- Added index culling optimization
- Fixed Intel (and AMD?) GPU crashes

# 0.0.1-alpha.1
- Added renderer
- Added color trigger support
- Added alpha trigger support
- Added move trigger support
- Added toggle trigger support
- Added object HSV
- Objects now rotate
- Pulsating objects now pulse to audio scale