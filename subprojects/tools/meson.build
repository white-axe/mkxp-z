project('tools', meson_version: '>=1.2.0')

python = import('python').find_installation(required: true)

embedtool = [python, files('embedtool.py')[0]]
quotetool = [python, files('quotetool.py')[0]]

host_triplet_cpu = host_machine.cpu()
host_triplet_vendor = 'unknown'
if host_machine.system() in ['windows', 'cygwin']
    host_triplet_os = 'mingw64'
else
    host_triplet_os = host_machine.system()
endif
host_triplet = host_triplet_cpu + '-' + host_triplet_vendor + '-' + host_triplet_os
