// Plugin.h
// AmiBroker plugin interface definitions for version 6.x+

#ifndef PLUGIN_H
#define PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <string.h>
#include <AmiBroker.h>

#define PLUGIN_VERSION 6.0

// Function declarations for the AmiBroker plugin interface
int __declspec(dllexport) PluginInfo(char *id, char *name, char *description, int *version);
int __declspec(dllexport) GetData(const char *symbol, const char *field, double *value);

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_H
