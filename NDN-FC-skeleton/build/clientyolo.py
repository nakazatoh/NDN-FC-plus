import socket
import base64
import sys
import time


#HOST = '172.17.0.1'
HOST = 'localhost'
PORT = 33115

with open(sys.argv[1], 'rb') as f:
    img = f.read()
    #print (base64.b64encode(img))

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:

    s.connect((HOST, PORT))

    #img = base64.b64encode(img).decode('utf-8')

    print ("Send to Docker")
    s.sendall(base64.b64encode(img))
    print ("complete")
    s.shutdown(1)
    print ("complete2")

    time.sleep(90)

    data = s.recv(300000)

    #print(data)

    receiveimg = base64.b64decode(data)
    
    f2 = open(sys.argv[1], 'bw')
    print (sys.argv[1])
    f2.write(receiveimg)
    f2.close()

    print ("Return from Docker")
