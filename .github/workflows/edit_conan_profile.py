import sys

if len(sys.argv) < 2:
    raise ValueError("no profile path provided") 

profile_path = sys.argv[1]

print(profile_path)

profile = dict()
current_section = ""
profile[current_section] = dict()

with open(profile_path, "r") as f:
    for line in f.readlines():
        stripped_line = line.strip()
        if len(stripped_line) == 0: continue
        if stripped_line[0] == '#': continue
        if stripped_line[0] == '[':
            current_section=stripped_line[stripped_line.find('[')+1:stripped_line.find(']')]
            profile[current_section] = dict()
            continue
        setting, value = stripped_line.split('=')
        profile[current_section][setting] = value

for arg in sys.argv[2:]:
    section = arg[0:arg.find('.')]
    setting, value = arg[arg.find('.')+1:].split('=')
    if section not in profile:
        profile[section] = dict()
    profile[section][setting] = value

with open(profile_path, "w") as f:
    for section, settings in profile.items():
        if section != "":
            f.write('\n['+section+"]\n")
        for setting, value in settings.items():
            f.write(setting+'='+value+'\n')
