# YootTower itemType (mItemType) Catalog

**Status:** Complete for the 105 PE-format plugins. The 51 SP-format plugins
(MV_* movies, Pl* events, Ob* objects) use an older container with no ATTR
resource and are not cataloged here.
**Source:** Extracted by sweeping every `data/Plugins/*.t2p` with pefile; the
mItemType and mPluginName come straight from each plugin's ATTR id=128. The
sweep is reproducible with the Python in `scratch/plugin_resource_audit.py`.

The YootTower plugin SDK (which once documented these 4cc codes) is no longer
available online, so this catalog is the authoritative list — derived directly
from the shipped plugins rather than from SDK docs.

## How the code is stored

The ATTR resource (id 128) stores `mItemType` as 4 raw bytes at offset 1. Read
in file order they form the "display" code (e.g. `NERH` = Apartment). The same
4 bytes as a little-endian u32 are the value the plugin DLL passes to its
item-class constructor — so `NERH` = `0x4852454E`. Case varies across the
population (mostly uppercase, but `EdtS`, `EgiB`, `EreS`, `VLEK` mix case).

## Catalog (sorted by code)

| itemType | Class     | Name (mPluginName)        | Notes |
|----------|-----------|---------------------------|-------|
| ABAB     | Tenant    | Barber Shop               |       |
| ACSE     | Mover     | Escalators                |       |
| ADEM     | Tenant    | Coin Press Machine        |       |
| AGEM     | Tenant    | Classic Movie Theatre     |       |
| ANIC     | Tenant    | Uncle Chow's              |       |
| AYIM     | Tenant    | Souvenirs                 |       |
| BNOC     | Tenant    | Convenience Store         |       |
| BOSS     | Tenant    | Soba Noodle Stand         |       |
| BNET     | Tenant    | Observatory               |       |
| BYKS     | Tenant    | Sky Bridge                |       |
| CFFO     | Tenant    | Office                    |       |
| CELE     | Tenant    | Electronic Store          |       |
| CUJK     | Tenant    | Juice Vending Machine     |       |
| CXEM     | Tenant    | Loco Taco                 |       |
| DOCR     | Tenant    | Record store              |       |
| DOFS     | Tenant    | Ahabs                     |       |
| DNOC     | Tenant    | Hawaiian Condo            |       |
| DNEH     | Tenant    | Power Room                |       |
| EBOK     | Tenant    | Small Room                |       |
| ECIK     | Tenant    | Ice Cream Stand           |       |
| EdtS     | Mover     | Simple Elevator           |       |
| EFAC     | Tenant    | coffee.com                |       |
| EKUT     | Tenant    | Local Pickle Farm         |       |
| EMAG     | Tenant    | Gameworks                 |       |
| ENIC     | Tenant    | Movie Theater             |       |
| EreS     | Mover     | Service Elevator          |       |
| ERAT     | Tenant    | Kids Store                |       |
| FNPJ     | Tenant    | Japanese Cuisine          |       |
| FRUS     | Tenant    | Surf Shop                 |       |
| GICK     | Tenant    | Cigarette Vending Machine |       |
| GICJ     | Tenant    | Cigarette Vending Machine |       |
| GNAD     | Tenant    | Japanese Snacks           |       |
| GURD     | Tenant    | Drug Store                |       |
| GRUB     | Tenant    | Burger land               |       |
| HCRF     | Tenant    | Eiffel Tower              |       |
| HTAC     | Tenant    | Cathedral                 | mPluginName is "Final" (placeholder) |
| IAWH     | World     | Hawaii                    | scenario plugin |
| IBEK     | Tenant    | Security Room             |       |
| IKAT     | Tenant    | Kegon Spa                 |       |
| IKAY     | Tenant    | Korean B-BQ               |       |
| IMAF     | Tenant    | Jenny's                   |       |
| IOKI     | Tenant    | Gathering Area            |       |
| IOTK     | Tenant    | Restroom                  |       |
| ISUS     | Tenant    | Sushi-Man                 |       |
| IUSH     | Tenant    | Suite rooms               |       |
| KAZI     | Tenant    | J-Pub                     |       |
| KCIT     | Tenant    | Ticket Booth              |       |
| KETS     | Tenant    | Big Country               |       |
| KLIM     | Tenant    | Juice Bar                 |       |
| KNDJ     | Tenant    | Juice Vending Machine     |       |
| KOOB     | Tenant    | Book Store                |       |
| KOHS     | Tenant    | Traditional Japanese      |       |
| KJNS     | World     | Tokyo                     | scenario plugin |
| KRAP     | Tenant    | Parking                   |       |
| KRAM     | Tenant    | Supermarket               |       |
| LATI     | Tenant    | Mama Pizano's             |       |
| LGSH     | Tenant    | Single rooms              |       |
| LIOT     | Tenant    | Restroom                  |       |
| LLIB     | OuterObj  | Billboard                 | exterior sprite |
| LTKC     | Tenant    | Cocktail Lounge           |       |
| LOCS     | Tenant    | Elementary School         |       |
| LPOT     | ToolPalette | Default Tool palette    | toolbox UI chrome |
| MDCB     | Tenant    | BCD-Mart                  |       |
| MUDS     | Tenant    | Stadium                   | mPluginName is "Final" (placeholder) |
| NMAR     | Tenant    | Ramen Noodle House        |       |
| NERH     | Tenant    | Apartment                 |       |
| NEMR     | Tenant    | Housekeeping              |       |
| NIES     | Tenant    | Fire Department           |       |
| NOGK     | World     | The Kegon Falls           | scenario plugin |
| NORF     | Tenant    | Front Desk                |       |
| NWTH     | Tenant    | Twin rooms                |       |
| ODIV     | Tenant    | Video & DVD               |       |
| ODNI     | Tenant    | Mahatma                   |       |
| OLFK     | Tenant    | Floor                     | K. (Kegan) variant |
| ORFR     | Tenant    | Front Desk                |       |
| OTEW     | Tenant    | Mrs.Weinstein'sToffee     |       |
| OHIH     | Tenant    | Final                     | placeholder (Attraction.t2p) |
| PKTB     | Tenant    | Boat Deck                 |       |
| PSOH     | Tenant    | Doctor's Office           |       |
| QTUB     | Tenant    | Boutique                  |       |
| RESK     | Mover     | K.F.Service Elevator      |       |
| RATS     | Mover     | Stairs                    |       |
| ROPS     | Tenant    | Sporting Goods            |       |
| ROLF     | Tenant    | Floor                     |       |
| SAHS     | Tenant    | Photo Studio              |       |
| SECI     | Tenant    | Ice Cream Stand           |       |
| SHLA     | Tenant    | Aloha Shop                |       |
| SIEK     | Tenant    | Old Time Cafe             |       |
| SNEM     | Tenant    | Men's Clothing Shop       |       |
| STEP     | Tenant    | Pet Shop                  |       |
| SUDK     | Tenant    | Trash Facility            |       |
| SYOT     | Tenant    | ToyWorld                  |       |
| TNEM     | Tenant    | Housekeeping              |       |
| TNVE     | Tenant    | Event Center              |       |
| TOCF     | Tenant    | Food Court                |       |
| TRPA     | Tenant    | Condo                     |       |
| TSUD     | Tenant    | Trash Facility            |       |
| TURF     | Tenant    | Fruit Stand               |       |
| UJRO     | Tenant    | Orange Julius             |       |
| VLEK     | Mover     | K.F.Simple Elevator       |       |
| WBUS     | Tenant    | Subway Station            |       |
| WOLF     | Tenant    | 1-800 FLOWERS             |       |
| XSFD     | Tenant    | Duty Free Shop            |       |
| YEBO     | Tenant    | Large Room                |       |

## Class distribution

| Class       | Count | DESC tag | Examples |
|-------------|-------|----------|----------|
| Tenant      | 93    | lPnT     | Apartment, Burger, Office, Condo |
| Mover       | 7     | lPvM     | Stairs, Escalator, elevators |
| World       | 3     | lPdW     | Hawaii, Tokyo, Kegon Falls |
| OuterObj    | 1     | lPoO     | Billboard |
| ToolPalette | 1     | LPOT     | Default Tool palette |

## Notes

- **Placeholder names:** `HTAC` (Cathedral), `MUDS` (Stadium), and `OHIH`
  (Attraction) ship with `mPluginName = "Final"` — the filename is the
  intended identity, which the catalog reflects.
- **K. variants:** `OLFK` (Floor), `RESK`/`VLEK` (elevators) are the
  Kegan-Falls scenario variants of standard items.
- **Duplicate display names:** several distinct codes map to the same
  in-game name (e.g. three "Cigarette Vending Machine": GICK/GICJ/GICK-locale,
  three "Restroom", two "Floor", two "Trash Facility", two "Front Desk",
  two "Housekeeping"). The code disambiguates them.
- The C++ lookup table is in `source/ItemTypeCatalog.h` (`kItemTypeCatalog`,
  `lookupItemType`), case-insensitive.
