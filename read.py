import numpy as np
import cv2
import glob
import sys

#scp -P4422 modfreitas@sc.npad.imd.ufrn.br:/home/modfreitas/parsec/parsec-3.0/ext/splash2x/apps/volrend/run/saida__* ~/Documentos/PCD/saidas

#data_path = os.path.join(img_dir,'*txt')
#files = glob.glob(data_path)
#data = []
#for f1 in files:
img = np.zeros((380, 380), dtype=np.uint8)
f = open(sys.argv[1], "r")
i = 0
for line in f.readlines():
    if(line == "\n" or line == ""):
        continue
              
    r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)
    print(r)
    try:
        #sim_large = 187
        img[i, 0:len(r)] = r
    except ValueError:        
        continue
    i += 1
    #data.append(img)

print(sys.argv[1].split(".")[0] + '.png')
cv2.imwrite(sys.argv[1].split(".")[0] + '.png', img)
cv2.waitKey(0)

#plt.imshow(img)
#plt.show()

cv2.destroyAllWindows()

f.close() 
