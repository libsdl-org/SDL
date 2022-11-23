#!/usr/bin/env python3
#
# Script to sort the game controller database entries in SDL_gamecontroller.c

import re


filename = "SDL_gamecontrollerdb.h"
input = open(filename)
output = open(f"{filename}.new", "w")
parsing_controllers = False
controllers = []
controller_guids = {}
conditionals = []
split_pattern = re.compile(r'([^"]*")([^,]*,)([^,]*,)([^"]*)(".*)')
guid_crc_pattern = re.compile(r'^([0-9a-zA-Z]{4})([0-9a-zA-Z]{2})([0-9a-zA-Z]{2})([0-9a-zA-Z]{24},)$')

def find_element(prefix, bindings):
    i=0
    for element in bindings:
        if element.startswith(prefix):
            return i
        i=(i + 1)

    return -1
       
def save_controller(line):
    global controllers
    match = split_pattern.match(line)
    entry = [ match.group(1), match.group(2), match.group(3) ]
    bindings = sorted(match.group(4).split(","))
    if (bindings[0] == ""):
        bindings.pop(0)

    crc = ""
    pos = find_element("crc:", bindings)
    if pos >= 0:
        crc = bindings[pos] + ","
        bindings.pop(pos)

    # Look for CRC embedded in the GUID and convert to crc element
    crc_match = guid_crc_pattern.match(entry[1])
    if crc_match and crc_match.group(2) != '00' and crc_match.group(3) != '00':
        print("Extracting CRC from GUID of " + entry[2])
        entry[1] = crc_match.group(1) + '0000' + crc_match.group(4)
        crc = "crc:" + crc_match.group(3) + crc_match.group(2) + ","

    pos = find_element("sdk", bindings)
    if pos >= 0:
        bindings.append(bindings.pop(pos))

    pos = find_element("hint:", bindings)
    if pos >= 0:
        bindings.append(bindings.pop(pos))

    entry.extend(crc)
    entry.extend(",".join(bindings) + ",")
    entry.append(match.group(5))
    controllers.append(entry)

    entry_id = entry[1] + entry[3]
    if ',sdk' in line or ',hint:' in line:
        conditionals.append(entry_id)

def write_controllers():
    global controllers
    global controller_guids
    # Check for duplicates
    for entry in controllers:
        entry_id = entry[1] + entry[3]
        if (entry_id in controller_guids and entry_id not in conditionals):
            current_name = entry[2]
            existing_name = controller_guids[entry_id][2]
            print("Warning: entry '%s' is duplicate of entry '%s'" % (current_name, existing_name))

            if (not current_name.startswith("(DUPE)")):
                entry[2] = f"(DUPE) {current_name}"

            if (not existing_name.startswith("(DUPE)")):
                controller_guids[entry_id][2] = f"(DUPE) {existing_name}"

        controller_guids[entry_id] = entry

    for entry in sorted(controllers, key=lambda entry: f"{entry[2]}-{entry[1]}"):
        line = "".join(entry) + "\n"
        line = line.replace("\t", "    ")
        if not line.endswith(",\n") and not line.endswith("*/\n") and not line.endswith(",\r\n") and not line.endswith("*/\r\n"):
            print("Warning: '%s' is missing a comma at the end of the line" % (line))
        output.write(line)

    controllers = []
    controller_guids = {}

for line in input:
    if parsing_controllers:
        if (line.startswith("{")):
            output.write(line)
        elif (line.startswith("    NULL")):
            parsing_controllers = False
            write_controllers()
            output.write(line)
        elif (line.startswith("#if")):
            print(f"Parsing {line.strip()}")
            output.write(line)
        elif (line.startswith("#endif")):
            write_controllers()
            output.write(line)
        else:
            save_controller(line)
    else:
        if (line.startswith("static const char *s_ControllerMappings")):
            parsing_controllers = True

        output.write(line)

output.close()
print(f"Finished writing {filename}.new")
