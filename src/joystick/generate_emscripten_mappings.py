file = open("SDL_gamepad_db.h")
result = []

def replace_subtype(mapping: str, new_subtype: str) -> str:
    split = mapping.split(',')
    guid = split[0]
    new_guid = guid[:30] + new_subtype
    split[0] = new_guid
    return ','.join(split)

def process_dinput_mapping(mapping: str) -> str:
    if "crc" in mapping:
        # Emscripten joystick backend is not CRC-compatible with DirectInput joystick backend yet
        return None
    # Replace the subtype field with "05" (Windows OS, joystick unmapped by the browser)
    return replace_subtype(mapping, "05")

def process_linux_mapping(mapping: str) -> str:
    if "crc" in mapping:
        # Emscripten joystick backend is not CRC-compatible with Linux joystick backend yet
        return None
    if "h0." in mapping:
        # On Linux in Firefox the hats get converted to axes, and we can't know for sure which ones here
        return None
    # Replace the subtype field with "02" (Linux OS, joystick unmapped by the browser)
    return replace_subtype(mapping, "02")

mapping_processor = None

for line in file.readlines():
    if line.startswith("#"):
        if line.strip() == "#ifdef SDL_JOYSTICK_DINPUT":
            mapping_processor = process_dinput_mapping
        elif "#if defined(SDL_JOYSTICK_LINUX)" in line:
            mapping_processor = process_linux_mapping
        else:
            mapping_processor = None
    elif '"' in line and ':' in line and mapping_processor != None:
        start = line.index('"')
        end = line.index("\",")
        clean_mapping = line[start+1:end]
        processed_mapping = mapping_processor(clean_mapping)
        if processed_mapping != None:
            result.append(f"\t\"{processed_mapping}\",\n")

file.close()

file = open("SDL_gamepad_db_emscripten.gen.h", 'w')
file.writelines(result)
