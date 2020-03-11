# reading txt file 
#source https://www.computerhope.com/issues/ch001721.htm
# Running python3 parse_data.py
import re
import os
#import matplotlib.pyplot as plt
################################################################
#SIMULATION PARAMETERS
noOfRuns     = 1
noOfFlows   = 0






mylines_list =[] # Declare an empty list 





#####################################################################
#FUNCTIONS

def getValuePerRun(infilepath, element):
    valuePerRun =[]
    with open("output.txt","rt") as myfile:  
        for line in myfile:
            m = re.search('PDR', line, re.IGNORECASE)
            if m != None:
                x = line.split(":")
                y = x[1].strip()
                valuePerRun.append(y)
                print(y)
        print(valuePerRun)


def getAverageOfRun(valuesPerRun):
        i = 0
        avg_Run = [];
        while i < noOfRuns:
                avg_Runs.append(sum(valuesPerRun[i]) / float(len(valuesPerRun[i])))
                i+=1

        return avg_Run


def getData(value,infilepath,outfilepath):
    runs = range(1,noOfuns)
    filename = "outfilepath"+"_Pdr" 	  
    if value == 'pdr':
        print("getData")
        flow = 1
        pdr_list = []
        while flow < noOfFlows + 1:
              elemName  = "PDR"+str(flow) 
              valuePerCycle = getValuePerCycle(infilepath,elenName)
              pdr = getAveragePerCycle(valuePerCycle)
              pdr_list.append(pdr)  
              flow +=1   
        with open("lava_1.txt","w+") as f:
            for i in range(noOfRuns-1):
                if noOfFlows == 1: 
                	f.write('%d \t%f\n' % (Runs[i], pdr_list[0][i]))
                elif noOfFlows == 2:
                    f.write('%d \t%f \t%f\n' % (Runs[i], pdr_list[0][i], pdr_list[1][i]))
                elif noOfFlows == 3: 
                    f.write('%d \t%f \t%f \t%f \n' % (Runs[i], pdr_list[0][i], pdr_list[1][i],pdr_list[2][i]))
                else :
                    f.write('%d \t%f \t%f \t%f \t%f \n' % (Runs[i], pdr_list[0][i], pdr_list[1][i],pdr_list[2][i],pdr_list[3][i]))
            i = 0
            steadyStates = []
            for i in  range(noOfFlows):
                pList =pdr_list[i]
                steadyState = pList[0] 
                for item in pList:
                    if (item >=  (steadyState + (steadyState * 0.05))) or (item <= (steadyState - (steadyState * 0.05))):
                        steadyState = item
                        steadyStates.append(steadyState)
                        ss = max(steadyStates)
                        steadyState ='steadyState=' + str(ss) 
 #   plot = "gnuplot -e " + "\"inFilename='" + filename +"';"+"StudyState='"+steadyState +"';" + "outFilename='" + filename + ".png';yLabel='PDR'\"" +" "+ "plotGraph.gp"      
  #  from os import system
  #  system(plot)
        f.close()




def main():
    inFilePath="output.txt"
    getValuePerRun(inFilePath,'PDR')


if __name__=="__main__":
    main()
            
