#!/usr/bin/python
import os
import string,re,sys

if __name__=='__main__':
    resF = open("fr.txt", "w")
    allFiles = os.listdir(".")
    #sort the list
    convert = lambda text: int(text) if text.isdigit() else text
    alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ]
    allFiles.sort( key=alphanum_key )
    #initialization
    loadT = []
    compT = []
    decodeT = []
    scaleT = []
    convT = []
    renderT = []
    afr = []
    hor = []
    ver = []
    thv = -1
    for logFN in allFiles:
        thv = -1
        if (string.find(logFN, ".py")!=-1):
            continue
        elif (string.find(logFN, ".swp")!=-1):
            continue
        elif (string.find(logFN, "log")!=-1):
            continue
        elif (string.find(logFN, "fr.txt")!=-1):
            continue
        print logFN
        tokens = re.split("_", logFN)
        print tokens[1]
        print tokens[2]
        for i in range(len(hor)):
            if (hor[i] == tokens[1] and ver[i] == tokens[2]):
                thv = i
                break
        print "***********" + str(thv)
        if (thv == -1):
            hor.append(tokens[1])
            ver.append(tokens[2])
        logF = file(logFN)
        for aLine in logF:
            print aLine
            tokens2 = re.split(": ", aLine)
            print tokens2[0]
            print tokens2[1]
            if (string.find(tokens2[0], "load")!=-1):
                if (thv == -1):
                    loadT.append(float(tokens2[1]))
                else:
                    loadT[thv] = loadT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "computation")!=-1):
                if (thv == -1):
                    compT.append(float(tokens2[1]))
                else:
                    compT[thv] = compT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "decode")!=-1):
                if (thv == -1):
                    decodeT.append(float(tokens2[1]))
                else:
                    decodeT[thv] = decodeT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "scale")!=-1):
                if (thv == -1):
                    scaleT.append(float(tokens2[1]))
                else:
                    scaleT[thv] = scaleT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "conversion")!=-1):
                if (thv == -1):               
                    convT.append(float(tokens2[1]))
                else:
                    convT[thv] = convT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "render")!=-1):
                if (thv == -1):
                    renderT.append(float(tokens2[1]))
                else:
                    renderT[thv] = renderT[thv] + float(tokens2[1])
            elif (string.find(tokens2[0], "rate")!=-1):
                if (thv == -1):
                    afr.append(float(tokens2[1]))
                else:
                    afr[thv] = (afr[thv] + float(tokens2[1]))
                    #print ")))))))))))" + str(afr[thv])
    for i in range(len(afr)):
        print afr[i]
    for i in range(len(hor)):
        resF.write(str(hor[i]) + ":" + str(ver[i]) + ":" + str(afr[i]/3) + "\n")
        
