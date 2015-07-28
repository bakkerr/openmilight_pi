
## Install

Install https://github.com/mysensors/Raspberry.git

Compile this source

## Usage 
```
Usage: sudo ./openmilight [hdfslun:p:q:r:c:b:k:v:w:]

   -h                       Show this help
   -d                       Show debug output
   -f                       Fade mode
   -s                       Strobe mode
   -l                       Listening (receiving) mode
   -u                       UDP mode
   -n NN<dec>               Resends of the same message
   -p PP<hex>               Prefix value (Disco Mode)
   -q RR<hex>               First byte of the remote
   -r RR<hex>               Second byte of the remote
   -c CC<hex>               Color byte
   -b BB<hex>               Brightness byte
   -k KK<hex>               Key byte
   -v SS<hex>               Sequence byte
   -w SSPPRRRRCCBBKKNN<hex> Complete message to send

 Author: Roy Bakker (2015)

 Inspired by sources from: - https://github.com/henryk/
                           - http://torsten-traenkner.de/wissen/smarthome/openmilight.php
```
