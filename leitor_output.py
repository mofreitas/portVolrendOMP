import numpy as np
import cv2
import glob
import sys
import os

VIDEO_MODE = False
IMAGE_FILE = ""
VIDEO_FOLDER = "./"
FRAMES = 100

#Para rodar, inseri-lo na pasta run da aplicação
#scp -P4422 modfreitas@sc.npad.imd.ufrn.br:/home/modfreitas/parsec/parsec-3.0/ext/splash2x/apps/volrend/run/saida__* ~/Documentos/PCD/saidas


if(len(sys.argv) == 2 and sys.argv[1]=="-v"):
    VIDEO_MODE = True
elif(len(sys.argv) == 3 and sys.argv[1]=="-v"):
    VIDEO_MODE = True
    VIDEO_FOLDER = sys.argv[2]
elif(len(sys.argv) == 5 and sys.argv[1]=="-v" and sys.argv[3]=="-f"):
    VIDEO_MODE = True
    VIDEO_FOLDER = sys.argv[2]
    try:
        FRAMES = int(sys.argv[4])
    except:
        print("Numero inválido de frames")
        exit(1)
elif(len(sys.argv) == 2):
    IMAGE_FILE = sys.argv[1]
else:
    print("USO:")
    print("Para geracao imagens: ArquivoTXT")
    print("Para geracao video: -v [pastaArquivosTXT] -f [frames]")
    exit(1)


if(VIDEO_MODE):
    files = [f for f in os.listdir(VIDEO_FOLDER) if f.endswith(".txt")]
    writer = cv2.VideoWriter('output.avi', cv2.VideoWriter_fourcc(*"FMP4"), 20.0, (640,480))
    if(len(files)==0):
        print("Não foram achado arquivos de textos nesta pasta")
        exit(1)

    for frameid ,file in enumerate(files):
        img = np.zeros((380, 380), dtype=np.uint8)
        i = 0
        pad_r = -1
        pad_l = -1
        pad_t = -1

        try:
            f = open(VIDEO_FOLDER+"/"+file, "r")          

            for line in f.readlines():
                if(line == "\n" or line == ""):
                    continue

                r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)   
                if(pad_l == -1):
                    pad_l = pad_t = int((380-len(r))/2)
                    pad_r = int(np.ceil((380-len(r))/2))

                img[i+pad_t, :] = np.pad(r, pad_width=(pad_l, pad_r), constant_values=(0, 0))            
                i += 1
                
            if(frameid > FRAMES):
                break;
         

            img = np.pad(img, ((int((480-380)/2), int((480-380)/2)), (int((640-380)/2), int((640-380)/2))), 'constant', constant_values=((0, 0), (0, 0)))
            torgb = cv2.cvtColor(img,cv2.COLOR_GRAY2RGB)
            print(f"Quadro {frameid}, {file}")
            writer.write(torgb)
            f.close()
        except:
            print("Arquivo de texto não existente: "+VIDEO_FOLDER)  
            exit(1) 
            
    print('Saída: output.avi')
    writer.release();
    cv2.destroyAllWindows() 
else:
    img = np.zeros((380, 380), dtype=np.uint8)
    try:
        f = open(IMAGE_FILE, "r")       
        i = 0
        pad_r = -1
        pad_l = -1
        pad_t = -1

        for line in f.readlines():
            if(line == "\n" or line == ""):
                continue
                                
            r = np.array(line.replace("\n", "0").split("|"), dtype=np.uint8)
            
            if(pad_l == -1):
                pad_l = pad_t = int((380-len(r))/2)
                pad_r = int(np.ceil((380-len(r))/2)) 
                          
            img[i+pad_t, :] = np.pad(r, pad_width=(pad_l, pad_r), constant_values=(0, 0))
            i += 1

        print("Saída: {}.png".format(IMAGE_FILE.split("/")[-1].split(".")[0]))
        cv2.imwrite(IMAGE_FILE.split("/")[-1].split(".")[0] + '.png', img)
        cv2.destroyAllWindows()
        f.close() 
    except:
        print("Arquivo não existente")  
        exit(1)

      
    
