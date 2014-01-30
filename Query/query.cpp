/*
	Query - Pings X36 driver.

	Based on Query.c - 1998 James M. Finnegan - Microsoft Systems Journal

*/
#include <windows.h>
#include <string.h>
#include <stdio.h>

void usage()
{
		printf("Usage: query profile.bpf to load a profile.\n");
		printf("       query -c to clear loaded profile.\n");
		printf("       query -a to clear calibration data.\n");
		printf("       query -p profile.bap to load an axis profile.\n");
		printf("       query -x to clear loaded axis profile.\n");
		printf("       query -l <axis> <location> to disable value floating.\n");
		printf("       query -f <axis> <location> to enable value floating.\n");
		printf("       query -v <axis> <location> to load value from current stick position.\n");
		printf("       query -z <axis> <location> <range> to set deadzone size.\n\n");
		printf("Axis:  1 = X, 2 = Y, 3 = Throttle, 4 = Rudder, 5 = Rotary 1, 6 = Rotary 2\n");
		printf("Location:  1 = min, 2 = center, 3 = max\n");
}


void main(int argc, char *argv[])
{
    char buffer[65535];    
    char szName[256];
	unsigned long j,k = 0;
    int i=0;
	HANDLE hDriver, hInput;
	char LoadProfile = 0;
	char ClearProfile = 0;
	char ClearCalib = 0;
	char ClearAxis = 0;
	char LoadAxis = 0;
	char Axis, Loc, DZ, type = 0;


    printf("X36 Device Test Program\n");

	if (argc < 2)
	{
		usage();
		exit(0);
	}


   
    sprintf(szName,"\\\\.\\%s", "X36Program");

	if (strcmp(argv[1],"-a") == 0)
	{
		if (argc != 2)
		{
			usage();
			exit(0);
		}
 
		ClearCalib = 1;
	}
	else if (strcmp(argv[1],"-c") == 0)
	{
		if (argc != 2)
		{
			usage();
			exit(0);
		}

		ClearProfile = 1;
	}
	else if ((strcmp(argv[1],"-l") == 0) || (strcmp(argv[1],"-f") == 0) || (strcmp(argv[1],"-v") == 0))
	{
		if (argc != 4)
		{
			usage(); exit(0);
		}

		Axis = atoi(argv[2]); Loc = atoi(argv[3]);
		if ((Axis < 1) || (Axis > 6) || (Loc < 1) || (Loc > 3))
		{
			usage(); exit(0);
		}

		switch(*(argv[1]+1))
		{
			case 'l':
				type = 1;
				break;
			case 'f':
				type = 2;
				break;
			case 'v':
				type = 3;
				break;
		}
	}
	else if (strcmp(argv[1],"-z") == 0)
	{
		if (argc != 5)
		{
			usage(); exit(0);
		}

		Axis = atoi(argv[2]); Loc = atoi(argv[3]); DZ = atoi(argv[4]);
		if ((Axis < 1) || (Axis > 6) || (Loc < 1) || (Loc > 3) || (DZ < 1) || (DZ > 127))
		{
			usage(); exit(0);
		}
	}
	else if (strcmp(argv[1],"-x") == 0)
	{
		if (argc != 2)
		{
			usage();
			exit(0);
		}

		ClearAxis = 1;
	}
	else if (strcmp(argv[1],"-p") == 0)
	{
		if (argc != 3)
		{
			usage();
			exit(0);
		}


		LoadAxis = 1;
		ClearAxis = 1;

		hInput = CreateFile(argv[2],
								GENERIC_READ,
								FILE_SHARE_READ,
								0,
								OPEN_EXISTING,
								0,
								0);

		if(hInput == INVALID_HANDLE_VALUE)
		{	printf("Error: unable to open file %s.   Error %ld\n", argv[1],GetLastError());
			printf("Usage: query -p profile.bap\n");
			return;
		}		
	}
	else
	{
		if (argc != 2)
		{
			usage();
			exit(0);
		}


		LoadProfile = 1;
		ClearProfile = 1;

		hInput = CreateFile(argv[1],
								GENERIC_READ,
								FILE_SHARE_READ,
								0,
								OPEN_EXISTING,
								0,
								0);

		if(hInput == INVALID_HANDLE_VALUE)
		{	printf("Error: unable to open file %s.   Error %ld\n", argv[1],GetLastError());
			printf("Usage: query profile.bpf\n");
			return;
		}
	}

    hDriver = CreateFile(szName,
                     GENERIC_READ | GENERIC_WRITE, 
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     0,                     // Default security
                     OPEN_EXISTING,
                     0,  
                     0);                    // No template

    // If the open failed, print out the error code
    if(hDriver == INVALID_HANDLE_VALUE)
	{
        printf("Error opening driver.  Error: %ld\n",
               GetLastError());
		CloseHandle(hInput);
	}
    else
    {	
		// Ping driver.
		buffer[0]=0;
		WriteFile(hDriver,buffer,1,&j,NULL);

		buffer[0]=1;
		ReadFile(hDriver,buffer,1,&j,NULL);
		if ((j != 1) || buffer[0] != 0)
		{
			printf("Ping error.  Value: %d\n",buffer[0]);
			CloseHandle(hDriver);
			CloseHandle(hInput);
			return;
		}
		else
			printf("Ping complete.\n");

		if (ClearCalib)
		{
			buffer[0]=4;
			WriteFile(hDriver,buffer,1,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Calibration clear error.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Calibration clear complete.\n");
		}


		if (ClearProfile)
		{

						buffer[0]=3;
			WriteFile(hDriver,buffer,1,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Profile clear error.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Profile clear complete.\n");

		}
		
		if (ClearAxis)
		{

			buffer[0]=7;
			WriteFile(hDriver,buffer,1,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Axis profile clear error.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Axis profile clear complete.\n");

		}
		

		if (type)
		{
			buffer[0]=5;
			WriteFile(hDriver,buffer,1,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Unable to communicate with driver.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}

			buffer[0]=Axis-1;
			buffer[1]=Loc-1;
			buffer[2]=type-1;
			WriteFile(hDriver,buffer,3,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Command fail.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Axis command complete.\n");
		}

		if (DZ)
		{
			buffer[0]=5;
			WriteFile(hDriver,buffer,1,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Unable to communicate with driver.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}

			buffer[0]=Axis-1;
			buffer[1]=Loc-1;
			buffer[2]=DZ | 0x80;
			WriteFile(hDriver,buffer,3,&j,NULL);

			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Command fail.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Axis deadzone set complete.\n");
		}


		if (LoadProfile || LoadAxis)
		{
			// Driver responding.  Configure to load profile.
			buffer[0]= (LoadAxis)?6:1;
			WriteFile(hDriver,buffer,1,&j,NULL);
			buffer[0]=1;
			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Error setting up profile load.  Value: %d\n",buffer[0]);
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("Driver open complete.\n");


			ReadFile(hInput,buffer, 65535, &k, NULL);
			if (!j)
			{
				printf("Error reading file.  Value: %d\n",GetLastError());
				CloseHandle(hDriver);
				CloseHandle(hInput);
				return;
			}
			else
				printf("File read complete.  Read %d bytes.\n",k);


			WriteFile(hDriver,buffer,k,&j,NULL);
			printf("Profile send complete.  Sent %d bytes.\n",j);

			ReadFile(hDriver,buffer,1,&j,NULL);
			if ((j != 1) || buffer[0] != 0)
			{
				printf("Profile load error.  Value: %d\n",buffer[0]);

			}
			else
				printf("Profile load complete.\n");

			CloseHandle(hInput);
		}

        CloseHandle(hDriver);
		
	}
}
