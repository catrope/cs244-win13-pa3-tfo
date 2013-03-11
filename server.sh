#! /bin/bash

PORT=12345

echo Starting server for Wikipedia experiment on port $PORT
cd wikipedia
../tfomultiserver -p 12345 -f -r resp

