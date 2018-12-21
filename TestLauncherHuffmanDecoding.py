import os 
import subprocess

directory = "gray8bit"
files = os.listdir(directory) 
for i in range ( len ( files ) ) :
    filename, file_extension = os.path.splitext(files[i])
    if(file_extension != ".pgm" and file_extension != ".Arc" and file_extension != ".Rlc"):
        filename2 = directory + "\\" + files[i]
        subprocess.Popen(["ComputerGraphic\\x64\\Release\\Huffman.exe", filename2])