# Collect stored logs from RuuviTag devices

With recent firmware, [RuuviTags](https://ruuvi.com/ruuvitag/) store some measurement data locally. 
This program collects the stored measurement data and can create a continuous data set even when there is no computer ("gateway") running continuously. 

The program runs on Linux only and uses [libblepp](https://github.com/edrosten/libblepp/) to communicate with the devices. 
