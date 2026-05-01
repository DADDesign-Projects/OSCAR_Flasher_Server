#pragma once
#include <winbase.h>
#pragma comment(lib, "OneCore.lib")

namespace Dad {
    // Class to enumerate communication ports
    class cEnumCommPorts {
    public:
        // Constructor initializes pointer to port array to nullptr
        cEnumCommPorts() {
            m_pTabPorts = nullptr;
        }

        // Destructor cleans up the allocated port array
        ~cEnumCommPorts() {
            if (m_pTabPorts != nullptr) {
                delete[] m_pTabPorts; // Deallocate memory for port array
                m_pTabPorts = nullptr; // Reset pointer to nullptr
            }
        }

        // Method to update the list of communication ports
        ULONG UpdateCommPorts() {
            if (m_pTabPorts != nullptr) {
                delete[] m_pTabPorts;                                       // Clean up existing port array
                m_pTabPorts = nullptr;                                      // Reset pointer to nullptr
            }
            // First pass for count COM ports
            ULONG PortNumbersCount = 0;                                     // Variable to hold the number of port numbers
            m_NbPortsFound = 0;                                             // Reset the number of found ports
            GetCommPorts(m_pTabPorts, PortNumbersCount, &m_NbPortsFound);   // Call to get port numbers (initial call)
            m_pTabPorts = new ULONG[m_NbPortsFound + 5];                    // Allocate new memory for port array
            PortNumbersCount = m_NbPortsFound + 4;                          // Update the count of port numbers to include extra buffer

            // Get number of each Port
            return GetCommPorts(m_pTabPorts, PortNumbersCount, &m_NbPortsFound); // Get the port numbers again
        }

        // Method to get the communication port number at a given index
        ULONG getNumComm(ULONG IndexPort, bool Refresh = false) {
            if ((m_pTabPorts == nullptr) || (Refresh == true)) {
                UpdateCommPorts();                                          // Refresh the list of communication ports if needed
            }
            if (IndexPort - 1 < m_NbPortsFound) {
                return m_pTabPorts[IndexPort - 1];                          // Return the port number at the given index
            }
            else {
                return 0;                                                   // Return 0 if the index is out of range
            }
        }

        // Method to get the number of found communication ports
        ULONG getNbPortsFound() {
            return m_NbPortsFound;
        }

    protected:
        ULONG* m_pTabPorts;         // Pointer to an array of port numbers
        ULONG m_NbPortsFound = 0;   // Number of found ports
    };
}
