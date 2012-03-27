#!/usr/bin/python
import os
import string,re,sys

if __name__ == '__main__':
    if (len(sys.argv) == 1):
        print "error"
        exit(0)
    m = sys.argv[1]
    n = sys.argv[2]
    x = sys.argv[3]
    os.rename("res.txt", "res_" + str(m) + "_" + str(n) + "_" + str(x) + ".txt") 
    os.rename("log.txt", "log_" + str(m) + "_" + str(n) + "_" + str(x) + ".txt")
