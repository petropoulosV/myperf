#!/bin/bash

stdbuf -oL ./server | stdbuf -oL grep Throughput | stdbuf -oL grep -Po '\d+\.\d+' | feedgnuplot --lines --points --stream
