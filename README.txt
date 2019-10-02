[BUILDING]
1. Edit CMakeLists.txt to specify allolib's location
2. Build as you want
3. excutables will be in ${BUILD_DIR}/bin
4. Make sure to run the app in project root dir so shader files are found (ex: ./build/bin/ssao)

[SSAO]
1. Hit space in the app to turn ambient occulsion on and off
2. left textures should intermediate steps, check them in the end of the onDraw function

[FXAA]
1. Hit space in the app to turn antialiasing on and off
