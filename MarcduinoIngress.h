#ifndef MARCDUINO_INGRESS_H
#define MARCDUINO_INGRESS_H

enum MarcduinoIngressTransportKind
{
    MARCDUINO_INGRESS_WEB_API,
    MARCDUINO_INGRESS_WEB_SOCKET,
    MARCDUINO_INGRESS_USB_SERIAL,
    MARCDUINO_INGRESS_BODY_LINK_UART,
    MARCDUINO_INGRESS_BODY_LINK_WIFI,
    MARCDUINO_INGRESS_WIFI_MARCDUINO,
    MARCDUINO_INGRESS_I2C_SLAVE,
    MARCDUINO_INGRESS_INTERNAL
};

struct MarcduinoIngressSource
{
    const char *label;
    MarcduinoIngressTransportKind transport;
    bool suppressBodyLinkEgress;
    bool fromRemotePeer;
};

static const MarcduinoIngressSource kMarcduinoIngressWebApi = {
    "astropixel-web-api",
    MARCDUINO_INGRESS_WEB_API,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressWebSocket = {
    "astropixel-web-ws",
    MARCDUINO_INGRESS_WEB_SOCKET,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressUsbSerial = {
    "usb-serial",
    MARCDUINO_INGRESS_USB_SERIAL,
    false,
    false
};

static const MarcduinoIngressSource kMarcduinoIngressBodyLinkUart = {
    "body-link-uart",
    MARCDUINO_INGRESS_BODY_LINK_UART,
    true,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressBodyLinkWifi = {
    "body-link-wifi",
    MARCDUINO_INGRESS_BODY_LINK_WIFI,
    true,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressWifiMarcduino = {
    "wifi-marcduino",
    MARCDUINO_INGRESS_WIFI_MARCDUINO,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressI2CSlave = {
    "i2c-slave",
    MARCDUINO_INGRESS_I2C_SLAVE,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressInternal = {
    "internal",
    MARCDUINO_INGRESS_INTERNAL,
    false,
    false
};

static const char *marcduinoIngressSourceLabel(const MarcduinoIngressSource &source)
{
    return source.label ? source.label : "unknown";
}

static bool marcduinoIngressSuppressesBodyLinkEgress(const MarcduinoIngressSource &source)
{
    return source.suppressBodyLinkEgress;
}

#endif // MARCDUINO_INGRESS_H
