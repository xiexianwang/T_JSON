import re

with open('src/mainwindow.cpp', 'r', encoding='utf-8') as f:
    mc = f.read()

# Make sure JsonFrameParser is included
if '#include "jsonframeparser.h"' not in mc:
    mc = mc.replace('#include "devicestate.h"', '#include "devicestate.h"\n#include "jsonframeparser.h"')

with open('src/mainwindow.cpp', 'w', encoding='utf-8') as f:
    f.write(mc)

print("JsonFrameParser included")
