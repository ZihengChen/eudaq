#include "eudaq/Configuration.hh"
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/ExampleHardware.hh"
#include <iostream>
#include <ostream>
#include <vector>

#include "eudaq/Status.hh"

// ubcm include
#include "ubcm.hpp"
#include "cratemaps.hpp"
#include "uhal/ConnectionManager.hpp"
#include <pugixml.hpp>


// A name to identify the raw data format of the events generated
// Modify this to something appropriate for your producer.
static const std::string EVENT_TYPE = "ubcm";

// Declare a new class that inherits from eudaq::Producer
class ubcmProducer : public eudaq::Producer {
  public:

    // The constructor must call the eudaq::Producer constructor with the name
    // and the runcontrol connection string, and initialize any member variables.
    ubcmProducer(const std::string & name, const std::string & runcontrol)
      : eudaq::Producer(name, runcontrol),
      _n_run(0), _n_ev(0), _stopping(false), _done(false) {}

    // This gets called whenever the DAQ is initialised
    virtual void OnInitialise(const eudaq::Configuration & init) {
      try {
        std::cout << "Reading: " << init.Name() << std::endl;
        
        // Do any initialisation of the ubcm here 
        // "start-up configuration", which is usally done only once in the beginning
        // Configuration file values are accessible as config.Get(name, default)
        _init_ubcmswDir   = init.Get("init_ubcmswDir", "/afs/cern.ch/user/z/zichen/public/BRIL/ubcm/sw");
        _init_fwDeployPy  = init.Get("init_fwDeployPy", "/scripts/deploy_fw.py");
        _init_fwDeployOpt = init.Get("init_fwDeployOpt", "-f");
        _init_fwDeployTgt = init.Get("init_fwDeployTgt", "lab.amc3");
        _init_fwDeployMcs = init.Get("init_fwDeployMcs", "../fw/fpga/build/bcm1f/bcm1f_0_14_154.mcs");
        _init_fwDeploySlotNumber = init.Get("init_fwDeploySlotNumber", "3");

        // Message as cout in the terminal of your producer
        std::cout << "Initialise with init_ubcmswDir = " << _init_ubcmswDir << std::endl;
        std::cout << "Initialise with init_fwDeployPy = " << _init_fwDeployPy << std::endl;
        std::cout << "Initialise with init_fwDeployOpt = " << _init_fwDeployOpt << std::endl;
        std::cout << "Initialise with init_fwDeployTgt = " << _init_fwDeployTgt << std::endl;
        std::cout << "Initialise with init_fwDeployMcs = " << _init_fwDeployMcs << std::endl;
        std::cout << "Initialise with init_fwDeploySlotNumber = " << _init_fwDeploySlotNumber << std::endl;
        // ---------- init your ubcm here ----------
        system("whoiam");

        std::string str_command = "python ";
        str_command += _init_ubcmswDir + _init_fwDeployPy + " ";
        str_command += _init_fwDeployOpt + " ";
        str_command += _init_fwDeployTgt + " ";
        str_command += _init_fwDeployMcs + " ";
        str_command += _init_fwDeploySlotNumber;

        const char *command = str_command.c_str(); 
        system(command);

        // send information
        // or to the LogCollector, depending which log level you want. These are the possibilities just as an ubcm here:
        EUDAQ_INFO("Initialise with init_ubcmswDir = " + _init_ubcmswDir);
        EUDAQ_INFO("Initialise with init_fwDeployPy = " + _init_fwDeployPy);
        EUDAQ_INFO("Initialise with init_fwDeployOpt = " + _init_fwDeployOpt);
        EUDAQ_INFO("Initialise with init_fwDeployTgt = " + _init_fwDeployTgt);
        EUDAQ_INFO("Initialise with init_fwDeployMcs = " + _init_fwDeployMcs);
        EUDAQ_INFO("Initialise with init_fwDeploySlotNumber = " + _init_fwDeploySlotNumber);

        // At the end, set the ConnectionState that will be displayed in the Run Control.
        // and set the state of the machine.
        SetConnectionState(eudaq::ConnectionState::STATE_UNCONF, "Initialised (" + init.Name() + ")");
      } 
      catch (...) {
        // Message as cout in the terminal of your producer
        std::cout << "Unknown exception" << std::endl;
        // Message to the LogCollector
        EUDAQ_ERROR("Error Message to the LogCollector from ubcmProducer");
        // Otherwise, the State is set to ERROR
        SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "Initialisation Error");
      }
    }

    // This gets called whenever the DAQ is configured
    virtual void OnConfigure(const eudaq::Configuration & config) {
      try {
        std::cout << "Reading: " << config.Name() << std::endl;

        // Do any configuration of the ubcm here
        // Configuration file values are accessible as config.Get(name, default)
        _conf_targetString = config.Get("conf_targetString", "bcm1f.crate2.amc3");
        _conf_connectionsRARP = config.Get("conf_connectionsRARP", "/afs/cern.ch/user/z/zichen/public/BRIL/ubcm/sw/cfg/connections_rarp.xml");
        _conf_maxNumberOfEvents = config.Get("conf_maxNumberOfEvents", 100); // for testing purpose only
        // Message as cout in the terminal of your producer
        std::cout << "ubcm conf_targetString = " << _conf_targetString << std::endl;
        std::cout << "ubcm conf_connectionsRARP = " << _conf_connectionsRARP << std::endl;
        std::cout << "ubcm conf_maxNumberOfEvents = " << _conf_maxNumberOfEvents << std::endl;


        // ---------- config your ubcm here ----------
        auto ipmi_connection_manager = std::make_shared<utca::ipmi_base>();
        uhal::setLogLevelTo( uhal::Warning() );
        uhal::ConnectionManager connection_manager( "file://"+_conf_connectionsRARP);

        // connect to all ubcm boards
        auto targetsNames = utca::load_connections( _conf_connectionsRARP, _conf_targetString);
        if ( targetsNames.empty() ){
          std::cerr << "No valid targets specified." << std::endl;
        } 
        
        // loop over crate
        for ( auto cr : targetsNames ) {
          std::cout << "Connecting to MCH " << cr.first << std::endl;
          // loop over amc boards
          for ( utca::amc_t amc : cr.second ) {
            std::cout << "Connecting to slot " << amc.slot << std::endl;

            // config ubcm on each amc boards in each crate
            utca::uBCM* this_ubcm;
            try {
              auto* ipmi = new utca::ipmi( ipmi_connection_manager, cr.first, amc.slot );
              this_ubcm = new utca::uBCM( *ipmi, connection_manager.getDevice( amc.ipbus_id ) );

              this_ubcm->init( utca::fw_BCM1F_FMC125, utca::quad_30spbx, 270 );
              this_ubcm->adc_bit_align();
              this_ubcm->adc_test_ramp();
                
              utca::GLIBv3::thresholds_t t{ 130, 130, 130, 130 };
              this_ubcm->configure_histograms( t, utca::GLIBv3::thresholds_t{ 1, 1, 1, 1 }, 4 );
              // this_ubcm->configure_raw_data( false, t );
              utca::GLIBv3::thresholds_t dt{ 18, 18, 18, 18 };
              utca::GLIBv3::thresholds_t to{ 2, 2, 2, 2 };
              utca::GLIBv3::thresholds_t at{ 12, 12, 12, 12 };
              this_ubcm->configure_peak_finder( dt, to, at );
              this_ubcm->m_carrier->configure_raw_data( utca::GLIBv3::trg_orbit );
              this_ubcm->m_carrier->configure_tlu_if( false, false );

            } catch ( std::exception& e ) {
              std::cerr << e.what() << std::endl;
              continue;
            }

            // check condition of ubcm on each amc boards in each crag
            std::string this_ubcm_is_ok = this_ubcm->check_system();
            std::cerr << "Check configuration: " << ( this_ubcm_is_ok.empty() ? "ubcm is OK." : this_ubcm_is_ok.c_str() ) << std::endl;
          
            // save this ubcm
            _targets.push_back(this_ubcm);
          }
        }





        // Message to the LogCollector
        EUDAQ_INFO("Configuring conf_targetString = " + _conf_targetString);
        EUDAQ_INFO("Configuring conf_connectionsRARP = " + _conf_connectionsRARP);
        EUDAQ_INFO("Configuring conf_maxNumberOfEvents = " + _conf_maxNumberOfEvents);

        

        // At the end, set the ConnectionState that will be displayed in the Run Control.
        // and set the state of the machine.
        SetConnectionState(eudaq::ConnectionState::STATE_CONF, "Configured (" + config.Name() + ")");
        
      } 
      catch (...) {
        // Otherwise, the State is set to ERROR
        printf("Unknown exception\n");
        SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "Configuration Error");
      }
    }

    // This gets called whenever a new run is started
    // It receives the new run number as a parameter
    // And sets the event number to 0 (internally)
    virtual void OnStartRun(unsigned param) {
      try {

        _n_run = param;
        _n_ev = 0;
      
        std::cout << "Start Run: " << _n_run << std::endl;

        // It must send a BORE (Begin-Of-Run Event) to the Data Collector
        eudaq::RawDataEvent bore(eudaq::RawDataEvent::BORE(EVENT_TYPE, _n_run));
        // You can set tags on the BORE that will be saved in the data file
        // and can be used later to help decoding
        // bore.SetTag("ubcm", eudaq::to_string(m_ubcmConfParam));
        // Starting your ubcm

        // ---------- start your ubcm here ----------









        // Send the event to the Data Collector
        SendEvent(bore);

        // At the end, set the ConnectionState that will be displayed in the Run Control.
        SetConnectionState(eudaq::ConnectionState::STATE_RUNNING, "Running");
      } 
      catch (...) {
        // Otherwise, the State is set to ERROR
        printf("Unknown exception\n");
        SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "Starting Error");
      }
    }

    // This gets called whenever a run is stopped
    virtual void OnStopRun() {
      try {
        // Set a flag to signal to the polling loop that the run is over and it is in the stopping process
        _stopping = true;

        // wait until all events have been read out from the ubcm
        while (_stopping) {
          eudaq::mSleep(20);
        }
        // Send an EORE after all the real events have been sent
        // You can also set tags on it (as with the BORE) if necessary
        SendEvent(eudaq::RawDataEvent::EORE(EVENT_TYPE, _n_run, ++_n_ev));
        
        // At the end, set the ConnectionState that will be displayed in the Run Control.
        // Due to the definition of FSM, it should go to STATE_CONF. 
        if (m_connectionstate.GetState() != eudaq::ConnectionState::STATE_ERROR)
          SetConnectionState(eudaq::ConnectionState::STATE_CONF);
      } 
      catch (...) {
        // Otherwise, the State is set to ERROR
        printf("Unknown exception\n");
        SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "Stopping Error");
      }
    }

    // This gets called when the Run Control is terminating,
    // we should also exit.
    virtual void OnTerminate() {
      std::cout << "Terminating..." << std::endl;
      _done = true;
    }

    // This loop is running in the main
    void ReadoutLoop() {
      try {
        // Loop until Run Control tells us to terminate using the done flag
        while (!_done) {

          if (_n_ev< _conf_maxNumberOfEvents) {

            if (GetConnectionState() != eudaq::ConnectionState::STATE_RUNNING) {
              // Now sleep for a bit, to prevent chewing up all the CPU
              eudaq::mSleep(20);
              // Then restart the loop
              continue;
            }

            // If we get here, there must be data to read out
            // Create a RawDataEvent to contain the event data to be sent
            eudaq::RawDataEvent ev(EVENT_TYPE, _n_run, _n_ev);

            for (unsigned itarget = 0; itarget < _targets.size(); ++itarget) {
              auto target = _targets.at(itarget);
              auto raw = target->read_raw_data( std::chrono::milliseconds( 100 ) );
              if ( !raw ) continue;
              std::cerr << "Trg#, " << raw->external_trigger_counter << ", TLU#, " << raw->external_trigger_data << '\n';
          
              for (unsigned ichannel = 0; ichannel < 4; ++ichannel) {
                auto buffer = raw->data[ichannel];
                // auto buffer = raw->data[ichannel].data();
                // std::vector<unsigned char> buffer = {0,1,2,3,4,5,6,7,7,8,9};
                // std::vector<unsigned char> buffer;
                // for ( char x: raw->data[i] ){
                //   buffer.push_back(x);
                // }
                ev.AddBlock(4*itarget+ichannel, buffer);
              }
            }


            // Send the event to the Data Collector      
            SendEvent(ev);
            // Now increment the event number
            _n_ev++;
            eudaq::mSleep(1000);
          }
        } 
      }
      catch (...) {
        // Otherwise, the State is set to ERROR
        printf("Unknown exception\n");
        SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "Error during running");
      }
    }
  public:
    std::vector<utca::uBCM*> _targets;

  private:
    unsigned _n_run, _n_ev;
    bool _stopping, _done;

    // ---------- init paramters ----------
    std::string _init_ubcmswDir;
    std::string _init_fwDeployPy;
    std::string _init_fwDeployOpt;
    std::string _init_fwDeployTgt;
    std::string _init_fwDeployMcs;
    std::string _init_fwDeploySlotNumber;

    // ---------- conf paramters ----------
    int _conf_maxNumberOfEvents;

    std::string _conf_targetString;
    std::string _conf_connectionsRARP;


};

// The main function that will create a Producer instance and run it
int main(int /*argc*/, const char ** argv) {
  // You can use the OptionParser to get command-line arguments
  // then they will automatically be described in the help (-h) option
  eudaq::OptionParser op("EUDAQ ubcm Producer", "1.0",
      "Just an ubcm, modify it to suit your own needs");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol",
      "tcp://localhost:44000", "address",
      "The address of the RunControl.");
  eudaq::Option<std::string> level(op, "l", "log-level", "NONE", "level",
      "The minimum level for displaying log messages locally");
  eudaq::Option<std::string> name (op, "n", "name", "ubcm", "string",
      "The name of this Producer");
  try {
    // This will look through the command-line arguments and set the options
    op.Parse(argv);
    // Set the Log level for displaying messages based on command-line
    EUDAQ_LOG_LEVEL(level.Value());
    // Create a producer
    ubcmProducer producer(name.Value(), rctrl.Value());
    // And set it running...
    producer.ReadoutLoop();
    // When the readout loop terminates, it is time to go
    std::cout << "Quitting" << std::endl;
  } catch (...) {
    // This does some basic error handling of common exceptions
    return op.HandleMainException();
  }
  return 0;
}




//EUDAQ_DEBUG("Debug Message to the LogCollector from ubcmProducer");
//EUDAQ_EXTRA("Extra Message to the LogCollector from ubcmProducer");
//EUDAQ_INFO("Info Message to the LogCollector from ubcmProducer");
//EUDAQ_WARN("Warn Message to the LogCollector from ubcmProducer");
//EUDAQ_ERROR("Error Message to the LogCollector from ubcmProducer");
//EUDAQ_USER("User Message to the LogCollector from ubcmProducer");
//EUDAQ_THROW("User Message to the LogCollector from ubcmProducer");