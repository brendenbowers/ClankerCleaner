#pragma once

namespace ClankerCleaner
{
    // The whole program: loads the settings, starts the pins and flows and runs until a stop
    // signal or every flow ends. Defined in main.cpp; each platform's EntryPoint.<Platform>.cpp
    // calls it.
    int Run(int argumentCount, char** arguments);
}
