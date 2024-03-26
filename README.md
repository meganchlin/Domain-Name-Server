# Domain Name Server

## Introduction

This project aims to implement a Domain Name Server (DNS) according to RFC 1035 specifications. The DNS server should be capable of correctly responding to DNS record queries for specific domain names. This README provides detailed instructions and specifications for implementing the DNS server.

## Specification Overview

### Objective

The primary objective is to implement a DNS server capable of handling DNS queries for specific domain names. Key behaviors to implement include reading a configuration file, responding to queries for handled domains, and forwarding requests for other domains.

### Protocol Messages

Implement standard query and response messages as defined in RFC 1035 and RFC 3596.

### Type Values

Implement required resource record types, including A, AAAA, NS, CNAME, SOA, MX, and TXT.

### Additional Functionality: A nip.io like service

Implement a service similar to nip.io, resolving any IP address from the domain.

### Configuration Files

The format of the configuration file is defined as follows.

<forwardIP>
<domain 1>,<path of zone file 1>
<domain 2>,<path of zone file 2>
...

### Zone Files

The format of a zone file containing records of a domain is defined as follows.

<domain>
<NAME>,<TTL>,<CLASS>,<TYPE>,<RDATA>
<NAME>,<TTL>,<CLASS>,<TYPE>,<RDATA>
...

The format of <RDATA> for each RR type is summarized below.
A : <ADDRESS>
AAAA : <ADDRESS>
NS : <NSDNAME>
CNAME : <CNAME>
SOA : <MNAME> <RNAME> <SERIAL> <REFRESH> <RETRY> <EXPIRE> <MINIMUM>
MX : <PREFERENCE> <EXCHANGE>
TXT : <TXT-DATA>

### DNS Client

Use the `dig` tool for DNS lookups to test the server.


## Usage

./dns <port-number> <path/to/the/config/file>

