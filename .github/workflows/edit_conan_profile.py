import sys

if len(sys.argv) < 2:
    raise ValueError("no profile path provided") 

profile_path = sys.argv[1]

profile = dict()
current_section = ""
profile[current_section] = (dict(),list())

with open(profile_path, "r") as f:
    for line in f.readlines():
        stripped_line = line.strip()
        if len(stripped_line) == 0: continue
        if stripped_line[0] == '#': continue
        if stripped_line[0] == '[':
            current_section=stripped_line[stripped_line.find('[')+1:stripped_line.find(']')]
            profile[current_section] = (dict(),list())
            continue
        if "=" in stripped_line:
            setting, value = stripped_line.split('=')
            profile[current_section][0][setting] = value
        else:
            profile[current_section][1].append(stripped_line)

for arg in sys.argv[2:]:
    section = arg[0:arg.find(':')]
    if section not in profile:
        profile[section] = (dict(),list())
    stripped_line = arg[arg.find(':')+1:]
    if "=" in stripped_line:
        setting, value = stripped_line.split('=')
        profile[current_section][0][setting] = value
    else:
        profile[current_section][1].append(stripped_line)

with open(profile_path, "w") as f:
    for section, settings in profile.items():
        if section != "":
            f.write('\n['+section+"]\n")
        for setting, value in settings[0].items():
            f.write(setting+'='+value+'\n')
        for value in settings[1].items():
            f.write(value+'\n')
