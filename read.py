import numpy as np
import cv2
import glob
import sys
import os

#Para rodar, inseri-lo na pasta run da aplicação

#scp -P4422 modfreitas@sc.npad.imd.ufrn.br:/home/modfreitas/parsec/parsec-3.0/ext/splash2x/apps/volrend/run/saida__* ~/Documentos/PCD/saidas

#data_path = os.path.join(img_dir,'*txt')
#files = glob.glob(data_path)
#data = []
#for f1 in files:

folder = "../../run/"
files = [f for f in os.listdir(folder) if f.endswith(".txt")]
writer = cv2.VideoWriter('output.avi', cv2.VideoWriter_fourcc(*"MJPG"), 10.0, (640,480))
for frameid ,file in enumerate(files):
    img = np.zeros((380, 380), dtype=np.uint8)
    f = open(folder+file, "r")
    i = 0
    for line in f.readlines():
        if(line == "\n" or line == ""):
            continue
                      
        r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)
        try:
            img[i, 0:len(r)] = r
        except ValueError:        
            continue
        i += 1
            
    img = np.pad(img.T, ((int((480-380)/2), int((480-380)/2)), (int((640-380)/2), int((640-380)/2))), 'constant', constant_values=((0, 0), (0, 0)))
    backtorgb = cv2.cvtColor(img,cv2.COLOR_GRAY2RGB)
    #img = np.tile(img, 3).T.reshape((480, 640, 3))
    print(f"Quadro {frameid}, {backtorgb.shape}")
    writer.write(backtorgb)
        
    #data.append(img)

#print(sys.argv[1].split(".")[0] + '.png')
#cv2.imwrite(sys.argv[1].split(".")[0] + '.png', img)
#cv2.waitKey(0)
writer.release();

#plt.imshow(img)
#plt.show()

cv2.destroyAllWindows()

f.close() 
