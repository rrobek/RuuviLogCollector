#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <iomanip>
#include <vector>
#include <optional>

#include <signal.h>

#include <stdexcept>

#include <blepp/logging.h>
#include <blepp/pretty_printers.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/lescan.h>

#include "constants.h"

using namespace std;
using namespace BLEPP;

void catch_function(int)
{
	cerr << "\nInterrupted!\n";
}

int handle_ruuvi(const string& dev) {
    cout << "Handle Ruuvi Tag: " << dev << endl;

    log_level = Debug;

    BLEGATTStateMachine gatt;

    //This is called when a complete scan of the device is done, giving
    //all services and characteristics. This one simply searches for the
    //standardised "name" characteristic and requests to read it.
    std::function<void()> found_services_and_characteristics_cb = [&gatt](){
        for(auto& service: gatt.primary_services) {
            cout << "  Service: " << to_str(service.uuid) << endl;
            for(auto& characteristic: service.characteristics) {
                cout << "     Char: " << to_str(characteristic.uuid) << endl;
                if(characteristic.uuid == UUID("2a00"))
                {
                    characteristic.cb_read = [&](const PDUReadResponse& r)
                    {
                        cout << "Hello, my name is: ";
                        auto v = r.value();
                        cout << string(v.first, v.second) << ". You killed my father. repare to die." << endl;
                        gatt.close();
                    };

                    characteristic.read_request();
                    goto name_found;
                }
            }
        }

        cerr << "No device name found." << endl;
        gatt.close();
        name_found:
        ;
    };

    //This is the simplest way of using a bluetooth device. If you call this
    //helper function, it will put everything in place to do a complete scan for
    //services and characteristics when you connect. If you want to save a small amount
    //of time on a connect and avoid the complete scan (you are allowed to cache this
    //information in certain cases), then you can provide your own callbacks.
    gatt.setup_standard_scan(found_services_and_characteristics_cb);

    //I think this one is reasonably clear?
    gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
    {
        if(d.reason != BLEGATTStateMachine::Disconnect::ConnectionClosed)
        {
            cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
            return 1;
        }
        else
            return 0;
    };

    //This is how to use the blocking interface. It is very simple. You provide the main
    //loop and just hammer on the state machine struct.
    gatt.connect_blocking(dev, BLEGATTStateMachine::ConnectMode::LE);
    for(;;)
        gatt.read_and_process_next();

}

int main(int argc, char** argv)
{
	HCIScanner::ScanType type = HCIScanner::ScanType::Active;
	HCIScanner::FilterDuplicates filter = HCIScanner::FilterDuplicates::Software;
	int c;
	string help = R"X(-[sHbdhp]:
  -s  software filtering of duplicates (default)
  -H  hardware filtering of duplicates 
  -b  both hardware and software filtering
  -d  show duplicates (no filtering)
  -h  show this message
  -p  passive scan
)X";
	while((c=getopt(argc, argv, "sHbdhp")) != -1)
	{
		if(c == 'p')
			type = HCIScanner::ScanType::Passive;
		else if(c == 's')
			filter = HCIScanner::FilterDuplicates::Software;
		else if(c == 'H')
			filter = HCIScanner::FilterDuplicates::Hardware;
		else if(c == 'b')
			filter = HCIScanner::FilterDuplicates::Both;
		else if(c == 'd')
			filter = HCIScanner::FilterDuplicates::Off;
		else if(c == 'h')
		{
			cout << "Usage: " << argv[0] << " " << help;
			return 0;
		}
		else 
		{
			cerr << argv[0] << ":  unknown option " << c << endl;
			return 1;
		}
	}


	log_level = LogLevels::Warning;
	HCIScanner scanner(true, filter, type);
	
	//Catch the interrupt signal. If the scanner is not 
	//cleaned up properly, then it doesn't reset the HCI state.
	signal(SIGINT, catch_function);

	//Something to print to demonstrate the timeout.
	string throbber="/|\\-";
	
	//hide cursor, to make the throbber look nicer.
	cout << "[?25l" << flush;

	int i=0;
	while (1) {
		

		//Check to see if there's anything to read from the HCI
		//and wait if there's not.
		struct timeval timeout;     
		timeout.tv_sec = 0;     
		timeout.tv_usec = 300000;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(scanner.get_fd(), &fds);
		int err = select(scanner.get_fd()+1, &fds, NULL, NULL,  &timeout);
		
		//Interrupted, so quit and clean up properly.
		if(err < 0 && errno == EINTR)	
			break;
		
		if(FD_ISSET(scanner.get_fd(), &fds))
		{
			//Only read id there's something to read
			vector<AdvertisingResponse> ads = scanner.get_advertisements();

			for(const auto& ad: ads)
			{
				cout << "Found device: " << ad.address << " ";

				if(ad.type == LeAdvertisingEventType::ADV_IND)
					cout << "Connectable undirected" << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_DIRECT_IND)
					cout << "Connectable directed" << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_SCAN_IND)
					cout << "Scannable " << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_NONCONN_IND)
					cout << "Non connectable" << endl;
				else
					cout << "Scan response" << endl;
                if(ad.local_name)
                    cout << "  Name: " << ad.local_name->name << endl;
                if(ad.rssi == 127)
                    cout << "  RSSI: unavailable" << endl;
                else if(ad.rssi <= 20)
                    cout << "  RSSI = " << (int) ad.rssi << " dBm" << endl;
                else
                    cout << "  RSSI = " << to_hex((uint8_t)ad.rssi) << " unknown" << endl;

                for(const auto& uuid: ad.UUIDs) {
					cout << "  Service: " << to_str(uuid) << endl;
                    if(uuid == nordic_uart_service_guid)
                        handle_ruuvi(ad.address);
                }
			}
		}
		else
			cout << throbber[i%4] << "\b" << flush;
		i++;
	}

	//show cursor
	cout << "[?25h" << flush;
}
