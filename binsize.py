#!/usr/bin/env python3

from subprocess import Popen, PIPE


class app(object):
    def __init__(self):
        pass


mgmt = app()
mgmt.name = 'mgmt'
mgmt.path = 'build/mgmt.elf'
mgmt.max_ram = 2*1024
mgmt.max_rom = 16*1024

APPS = [mgmt]

for app in APPS:

    ram = 0
    rom = 0

    p = Popen(['arm-none-eabi-size', '-A', app.path], stdout=PIPE)

    if p.wait() == 0:
        lines = [str(l.strip()) for l in p.stdout if len(l)]
        lines = lines[2:-3]

        for line in lines:
            columns = [c for c in line.split(' ') if len(c)]

            if 'stack' in columns[0]:
                ram += int(columns[1])
            elif '.ram0' in columns[0]:
                ram += int(columns[1])
                rom += int(columns[1])
            elif '.bss' in columns[0]:
                ram += int(columns[1])
            elif '.data' in columns[0]:
                ram += int(columns[1])
                rom += int(columns[1])
            elif '.rodata' in columns[0]:
                rom += int(columns[1])
            elif '.text' in columns[0]:
                rom += int(columns[1])
            elif '.vectors' in columns[0]:
                rom += int(columns[1])

        print('')
        print(app.name)

        print('RAM used: {:3.2f}% - {:3.2f} / {:.0f}K'.format((ram*100)/app.max_ram,
                                                   ram/1024.0,
                                                   app.max_ram/1024.0))

        print('ROM used: {:3.2f}% - {:3.2f} / {:.0f}K'.format((rom*100)/app.max_rom,
                                                   rom/1024.0,
                                                   app.max_rom/1024.0))
