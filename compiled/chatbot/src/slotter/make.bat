compile -6 dll Slotter.cpp /Fe"ChatBot.dll" /link /EXPORT:init
::cl TestApi2.cpp /Fe"ChatBot.dll" /O1 /MD /link /DLL /EXPORT:init
