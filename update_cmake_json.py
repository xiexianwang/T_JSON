import sys
import re

with open('CMakeLists.txt', 'r', encoding='utf-8') as f:
    cm = f.read()

# Add jsonframeparser.cpp
target = '    src/configmanager.h'
insert = '    src/jsonframeparser.cpp\n    src/jsonframeparser.h\n    src/devicestate.h\n'

if 'jsonframeparser.cpp' not in cm:
    cm = cm.replace(target, target + '\n' + insert)
    with open('CMakeLists.txt', 'w', encoding='utf-8') as f:
        f.write(cm)
    print("CMakeLists.txt updated")
else:
    print("already updated")
