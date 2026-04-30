# Source - https://stackoverflow.com/a/18274231
# Posted by 6160, modified by community. See post 'Timeline' for change history
# Retrieved 2026-04-30, License - CC BY-SA 4.0
import os

yourpath = '../recordings/wav_files/'

def load_file_paths(l):
    for root, dirs, files in os.walk(yourpath, topdown=False):
        for name in files:
            l.append(os.path.join(root, name))
    return l
