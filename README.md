# EasyTier-PeerDiscovery

An experimental P2P discovery helper for EasyTier. It uses the BitTorrent
Mainline DHT to discover EasyTier peers without pre-configuring another member
node, then exposes them through EasyTier's HTTP Manual Discovery interface.

> **CAUTION:** Vibe-coded & experimental project. Do not rely on this project
> for production or security-sensitive deployments without independent review.

## Usage

Start the discovery helper:

```sh
easytier-peerdisc --network-name <network-name> --secret <secret>
```

`ET_NETWORK_NAME` and `ET_NETWORK_SECRET` are accepted as alternatives to
`--network-name` and `--secret`, respectively. Command-line values take
precedence.

Point EasyTier at its HTTP endpoint:

```sh
easytier-core -n <network-name> -k <secret> \
    -p http://127.0.0.1:11008/endpoint
```

See `easytier-peerdisc --help` and [`doc/program_argv.md`](doc/program_argv.md)
for available options.

## Security and privacy risks

This project uses the public BitTorrent Mainline DHT. Bootstrap makes joining
a network convenient, but anyone who knows or obtains the network's InfoHash
can discover announced public node IP addresses and ports.

DHT crawlers or other third parties may attempt to connect to announced
EasyTier nodes. Treat the DHT as an untrusted public directory: use a strong
network secret, expose only intended listeners, and apply appropriate host and
network firewall rules. The InfoHash is derived from the network name and an
Argon2id-hashed network secret. Knowing an InfoHash does not itself grant
access to the EasyTier network, unless the network secret is weak enough to be
guessed or cracked.
