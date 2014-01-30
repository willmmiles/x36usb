// Fake HID data.

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

HID_REPORT_DESCRIPTOR           MyReportDescriptor[] = {
	
	// Original X36 descriptor, modified for new axis reporting mechanism
	0x05, 0x01,			// Usage Page (Generic Desktop),
	0x09, 0x04,			// Usage Type Joystick
	0xa1, 0x01,			// Collection - Application
	0x85, 0x01,   // Report ID 01.
	
	0x09, 0x30,			// Usage Xaxis
	0x17, 0x00, 0x00, 0x00,	0x00, // Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// Report count 1
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field

	0x09, 0x31,			// Usage Yaxis	
	0x17, 0x00, 0x00, 0x00,	0x00,	// Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// Report count 1
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field

	0x05, 0x02,			// Usage page (simulation)
	0x09, 0xBB,		// Usage Throttle	
	//0x09, 0x32,			// Usage Zaxis
	//0x09, 0x36,		// Usage Slider
	0x17, 0x00, 0x00, 0x00,	0x00,	// Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// 1 item
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field
	
	
	0x09, 0xBA,			// Usage Zrotation (rudder)
	0x17, 0x00, 0x00, 0x00,	0x00,	// Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// 1 item
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field
	
	0x05, 0x01,			// Usage Page (Generic Desktop),
	//0x09, 0x37,			// Usage Dial (rotary 1)
	0x09, 0x33,			// Usage Xrotation (rotary 1)
	0x17, 0x00, 0x00, 0x00,	0x00,	// Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// 1 item
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field
	
	0x09, 0x34,			// Usage Yrotation (rotary 2)
	0x17, 0x00, 0x00, 0x00,	0x00,	// Logical minimum 0x0057
	0x27, 0xff, 0xff, 0x00,	0x00, 	// Logical maximum 0x0161
	0x75, 0x20,			// Report size 32 bits
	0x95, 0x01,			// 1 item
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field
	
	0x05, 0x09,			// Usage page Buttons
	0x15, 0x00,			// Logical minimum 0
	0x25, 0x01,			// Logical maximum 1
	0x19, 0x01,			// Usage minimum 01
	//0x29, 0x3F,			// Usage maximum 64 (buttons 1 to 64)
	0x29, 0x1F,			// 31 buttons
	0x75, 0x01,			// Report size one bit
	//0x95, 0x3F,			// Report length 64 items
	0x95, 0x1F,			// 31 buttons
	0x81, 0x02,			// Input - Data, Variable, Absolute, Wrap, Linear, Preferred, No Null, Bit Field
	
						// Empty space - 4+(8-(num_buttons%8))
	0x75, 0x05,			// Report size
	0x95, 0x01,			// Report length 1 entry
	0x81, 0x01,			// Input - Constant, Array, Absolute, No Wrap, Linear, Preferred, No Null 

	0x05, 0x01,			// Usage page appplication
	0x09, 0x39,			// Usage Hat Switch
	0x15, 0x01,			// Logical minimum 1
	0x25, 0x08,			// Logical maximum 8
	0x35, 0x00,			// Physical minimum 0
	0x46, 0x3b,	0x01,	// Physical maximum 0x013b (315 degrees?)
	0x66, 0x14, 0x00,	// Unit values 0x0014 (20 degrees?)
	0x75, 0x04,			// 4 bits
	0x95, 0x01,			// 1 copy
	0x81, 0x42,			// Input - Data, Variable, Absolute, No Wrap, Linear, Preferred, Null State
	0xc0,				// End collection
	
	// Keyboard descriptor
    0x05,   0x01,       // Usage Page (Generic Desktop),
    0x09,   0x06,       // Usage (Keyboard),
    0xA1,   0x01,       // Collection (Application),
    0x85, 0x02,   // Report ID 02.
    0x05,   0x07,       //  Usage Page (Key Codes);
    0x19,   0x00,       //  Usage Minimum (0),
    0x29,   0xE7,       //  Usage Maximum (231),
    0x15,   0x00,       //  Logical Minimum (0),
    0x25,   0x01,       //  Logical Maximum (1),
    0x75,   0x01,       //  Report Size (1),
    0x95,   0xE8,       //  Report Count (232),
    0x81,   0x02,       //  Input (Data, Variable, Absolute),;Modifier byte

    0xC0,                // End Collection


	// Mouse Descriptor
    0x05, 0x01, // Usage Page (Generic Desktop),
    0x09, 0x02, // Usage (Mouse),
    0xA1, 0x01, // Collection (Application),
0x85, 0x03,   // Report ID 02.
    0x09, 0x01, // Usage (Pointer),
    0xA1, 0x00, // Collection (Physical),
    0x05, 0x09, // Usage Page (Buttons),
    0x19, 0x01, // Usage Minimum (01),
    0x29, 0x03, // Usage Maximun (03),
    0x15, 0x00, // Logical Minimum (0),
    0x25, 0x01, // Logical Maximum (1),
    0x95, 0x03, // Report Count (3),
    0x75, 0x01, // Report Size (1),
    0x81, 0x02, // Input (Data, Variable, Absolute), ;3 button bits
    0x95, 0x01, // Report Count (1),
    0x75, 0x05, // Report Size (5),
    0x81, 0x01, // Input (Constant), ;5 bit padding
    0x05, 0x01, // Usage Page (Generic Desktop),
    0x09, 0x30, // Usage (X),
    0x09, 0x31, // Usage (Y),
    0x16, 0x00, 0x00, // Logical Minimum (-127),
    0x26, 0xFF, 0xFF, // Logical Maximum (127),
    0x75, 0x10, // Report Size (8),
    0x95, 0x02, // Report Count (2),
    0x81, 0x06, // Input (Data, Variable, Relative), ;2 position bytes (X & Y)
    0xC0, // End Collection,
    0xC0 // End Collection

};


























