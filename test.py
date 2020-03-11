# reading txt file 
#source https://www.computerhope.com/issues/ch001721.htm
# Running python3 parse_data.py
import re
################################################################
#SIMULATION PARAMETERS

def main():
    with open("test_open.txt","w+") as myfile:  
        for line in myfile:
            m = re.search('PDR', line, re.IGNORECASE)
            if m != None:
                x = line.split(":")
                print("Listx[0]")
                print(x[0])
                y=x[1].strip()

                print("Listy")
                print(y)

if __name__=="__main__":
    main()
            
