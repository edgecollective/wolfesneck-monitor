#define this_node_id 2 // remote node must have node_id > 1 (1 is the gateway)

const int sleep_interval = 2000; //milliseconds // BUG -- why can't we set this to 5000 w/ wake_counter_max > 2?

#define watchdog_interval 15000 // milliseconds

//int wake_counter_max = 300; // for 10 min

int wake_counter_max = 5; // for 10 min

const int interval_sec = 5;

// don't need to modify this unless you have more than one gateway within lora range
const char* loranet_pubkey = "zgpqdys5a9r3"; //this will be 'network A'; we can add more networks later if need be
