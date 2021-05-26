rm log
./waf --run "scratch/project --speed_max=30   --speed_min=0 --n_nodes=12" >>log

vi log
