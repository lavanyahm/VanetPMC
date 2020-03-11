# reading txt file 
#source https://www.computerhope.com/issues/ch001721.htm
# Running python3 parse_data.py
import re
################################################################
#SIMULATION PARAMETERS
noOfRuns = 1






mylines_list =[] # Declare an empty list 





#####################################################################
#FUNCTIONS

def getValuePerRun(infilepath, element):
    valuePerRun ={}
    with open("output.txt","rt") as myfile:  
        for line in myfile:
            str = myline
            m = re.search('PDR\s+=\s[.0-9]+)',line)
            if m:
               Pdr = float(m.group(1)) 
               print("Found PDR")
                


def getAverageOfRun(valuesPerRun):
        i = 0
        avg_Run = [];
        while i < noOfRuns:
                avg_Runs.append(sum(valuesPerRun[i]) / float(len(valuesPerRun[i])))
                i+=1

        return avg_Run



def main():
    with open("output.txt","rt") as myfile:  
        for myline in myfile:
            str = myline
            pat = re.compile(r"\bP\w*R\b",re.IGNORECASE)
            if pat.search(str) !=None:
                print(str)
                m = re.match(r"(\w+) (\w+)", "PDR")
                print (m.group(0))

if __name__=="__main__":
    main()
            
