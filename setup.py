from setuptools import setup, find_packages, Extension
from subprocess import Popen, PIPE, STDOUT

# Based on http://code.activestate.com/recipes/502261-python-distutils-pkg-config/
# but replaced commands.getoutput with subprocess and some other minor fixes.
# All to get it working with python3
def pkgconfig(*packages, **kw):
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}

    # run pkg-config
    args = ['pkg-config', '--libs', '--cflags'] + list(packages)
    proc = Popen(args, stdout=PIPE, stderr=STDOUT)
    output = proc.communicate()[0]
    
    if proc.returncode != 0:
        print >> sys.stderr, output
        sys.exit(1)

    # parse output
    for token in output.split():
        token = token.decode()
        if token[:2] in flag_map:
            kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
            
        else: # throw others to extra_link_args
            kw.setdefault('extra_link_args', []).append(token)


    for k, v in kw.items(): # remove duplicated
        kw[k] = list(set(v))

    return kw

setup(
    name = "xorg_query",
    version = '0.2',
    ext_modules = [
        Extension('xorg_query', ['query.c'], **pkgconfig('x11', 'xrandr'))
    ],
)
