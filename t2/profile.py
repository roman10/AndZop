#!/usr/bin/python
import string,re,sys
# if input parameter is 1, then it ignores dependency loading and computation time
# 
if __name__ == '__main__':
        ignoreDep = False
        ignoreComp = False
        if (len(sys.argv) > 1):
	    ignoreDep = True
        if (len(sys.argv) > 2):
            ignoreComp = True
	resF = open("res.txt", "w")
	logF = file( "log.txt" )
	renderT = []
	decodeT = []
	scaleT = []
	colorCT = []
	depCT = []
	depLT = []
        compT = []
        totalTime = 0.0
        totalFrs = 0
	sms = 0
	shh = 0
	smm = 0
	sss = 0
	ems = 0
	ehh = 0
	emm = 0
	ess = 0
	diff = 0
	for aLine in logF:
		token = re.split(": ", aLine)[0]
		token = re.split(" ", token)[1]
		token = string.rstrip(token, ":")
		#print token
		ts = re.split("\.", token)
		#print ts[0]
		if ("ST" in aLine):
			sms = int(ts[1])
			ts = re.split(":", ts[0])
			shh = int(ts[0])
			smm = int(ts[1])
			sss = int(ts[2])
			#print sms + " " + shh + " " + smm + " " + sss 
		elif ("ED" in aLine):
			ems = int(ts[1])
			ts = re.split(":", ts[0])
			ehh = int(ts[0])
			emm = int(ts[1])
			ess = int(ts[2])
			#print ems + " " + ehh + " " + emm + " " + ess 
			if (shh > ehh):
				ehh = ehh + 24
			diff = ((ehh - shh)*3600 + (emm - smm)*60 + (ess - sss))*1000 + (ems - sms)
		if ("---LD ED" in aLine):
			depLT.append(diff)
		if ("---RENDER ED" in aLine):
			renderT.append(diff)
		if ("---CMP ED" in aLine):
			depCT.append(diff)
		if ("---DECODE ED" in aLine):
			decodeT.append(diff)
		if ("COLOR ED" in aLine):
			colorCT.append(diff)
		if ("SCALE ED" in aLine):
			scaleT.append(diff)
                if ("---COMPOSE ED" in aLine):
			compT.append(diff)
                if ("---FR" in aLine):
                        token2 = re.split(": ", aLine)
                        token2 = re.split(":", token2[1])
#                        print token2[1]
#                        print token2[2]
                        totalTime = float(token2[1])
                        totalFrs = int(token2[2])
	totalRenderT = 0.0
	totalDecodeT = 0.0
	totalColorCT = 0.0
	totalDepCT = 0.0
	totalDepLT = 0.0
	totalScaleT = 0.0
        totalCompT = 0.0
	if (ignoreDep == False):
		for i in range(len(depLT)):
			totalDepLT = totalDepLT + depLT[i]
		resF.write("dependency loading time: " + str(totalDepLT/len(depLT)) + "\n")
		for i in range(len(depCT)):
			totalDepCT = totalDepCT + depCT[i]
		resF.write("dependency computation time: " + str(totalDepCT/len(depCT)) + "\n")
	for i in range(len(decodeT)):
		totalDecodeT = totalDecodeT + decodeT[i]
	resF.write("decode time: " + str(totalDecodeT/len(decodeT)) + "\n")
	for i in range(len(scaleT)):
		totalScaleT = totalScaleT + scaleT[i]
	resF.write("scale time: " + str(totalScaleT/len(scaleT)) + "\n")
	for i in range(len(colorCT)):
		totalColorCT = totalColorCT + colorCT[i]
	resF.write("color conversion time: " + str(totalColorCT/len(colorCT)) + "\n")
	for i in range(len(renderT)):
		totalRenderT = totalRenderT + renderT[i]
	resF.write("render time: " + str(totalRenderT/len(renderT)) + "\n")
        if (ignoreComp == False):
   	    for i in range(len(compT)):
		    totalCompT = totalCompT + compT[i]
            resF.write("compose packet time: " + str(totalCompT/len(compT)) + "\n")

        resF.write("avg frame rate: " + str(totalFrs/(totalTime/1000)) + "\n");
	resF.close()
	logF.close()
			
			
			
			
