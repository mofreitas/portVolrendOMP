import numpy as np
import cv2
import glob
import sys
import os

#Para rodar, inseri-lo na pasta run da aplicação
#scp -P4422 modfreitas@sc.npad.imd.ufrn.br:/home/modfreitas/parsec/parsec-3.0/ext/splash2x/apps/volrend/run/saida__* ~/Documentos/PCD/saidas

if(len(sys.argv) == 2 and sys.argv[1]=="-v"):
    folder = "../../run/"
    files = [f for f in os.listdir(folder) if f.endswith(".txt")]
    writer = cv2.VideoWriter('output.avi', cv2.VideoWriter_fourcc(*"MJPG"), 20.0, (640,480))
    for frameid ,file in enumerate(files):
        img = np.zeros((380, 380), dtype=np.uint8)
        f = open(folder+file, "r")
        i = 0
        pad_r = -1
        pad_l = -1

        for line in f.readlines():
            if(line == "\n" or line == ""):
                continue
                        
            r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)   

            if(pad_l == -1):
                pad_l = int((380-len(r))/2)
                pad_r = int(np.ceil((380-len(r))/2))

            img[i, :] = np.pad(r, pad_width=(pad_l, pad_r), constant_values=(0, 0))
            
            i += 1
            
        if(frameid > 100):
            break;

        img = np.pad(img, ((int((480-380)/2), int((480-380)/2)), (int((640-380)/2), int((640-380)/2))), 'constant', constant_values=((0, 0), (0, 0)))
        torgb = cv2.cvtColor(img,cv2.COLOR_GRAY2RGB)
        print(f"Quadro {frameid}, {file}")
        writer.write(torgb)
            
    print('Saída: output.avi')
    writer.release();
    cv2.destroyAllWindows()
    f.close() 
elif(len(sys.argv)):
    file = sys.argv[1]
    img = np.zeros((380, 380), dtype=np.uint8)
    try:
        f = open(file, "r")
        i = 0
        pad_r = -1
        pad_l = -1
        for line in f.readlines():
            if(line == "\n" or line == ""):
                continue
                            
            r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)
            
            if(pad_l == -1):
                pad_l = int((380-len(r))/2)
                pad_r = int(np.ceil((380-len(r))/2))

            img[i, :] = np.pad(r, pad_width=(pad_l, pad_r), constant_values=(0, 0))
            i += 1

        print("Saída: {}.png".format(sys.argv[1].split(".")[0]))
        cv2.imwrite(sys.argv[1].split(".")[0] + '.png', img)
        cv2.destroyAllWindows()
        f.close() 
    except:
        print("Arquivo não existente")    
    
else:
    print("Parâmetros incorretos")
    print("-v: Processa todos os arquivos em um vídeo")
    print("<nome_arquivo>: Processa todo em uma imagem")
