"""Cross build with the locally extracted Debian MinGW toolchain."""
from pathlib import Path
import subprocess, os
base = Path(__file__).resolve().parent
root = base / '.toolchain/root/usr'
env = os.environ.copy()
env['PATH'] = str(root / 'bin') + ':' + env['PATH']
gcc = str(root / 'bin/x86_64-w64-mingw32-gcc-posix')
inc = ['-isystem', str(root/'x86_64-w64-mingw32/include')]
(base/'dist').mkdir(exist_ok=True)
# Preprocess separately: windres shell parsing does not handle a tool path containing spaces.
with (base/'dist/app.pre.rc').open('wb') as output:
    subprocess.run([gcc,'-E','-xc','-DRC_INVOKED',*inc,str(base/'resources/app.rc')],env=env,stdout=output,check=True)
subprocess.run([str(root/'bin/x86_64-w64-mingw32-windres'),'-I',str(base/'resources'),str(base/'dist/app.pre.rc'),str(base/'dist/app-res.o')],env=env,check=True)
for role in ['client','server']:
    cmd=[gcc,*inc,'-I'+str(base/'include'),'-L'+str(root/'x86_64-w64-mingw32/lib'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-DWINVER=0x0A00','-municode','-mwindows','-static','-static-libgcc','-Wl,--dynamicbase,--nxcompat',str(base/'src/window.c'),str(base/'src/session.c'),str(base/'src/chat.c'),str(base/'src/screen.c'),str(base/'dist/app-res.o'),'-lws2_32','-lcomctl32','-lgdi32','-luser32','-o',str(base/'dist'/('assist_'+role+'.exe'))]
    if role=='server': cmd.insert(1,'-DASSIST_HOST=1')
    subprocess.run(cmd,env=env,check=True)
