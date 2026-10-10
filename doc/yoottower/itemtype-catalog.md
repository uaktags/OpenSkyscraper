# YootTower plugin id / item type catalog

**Status:** Complete for the 105 PE-format plugins. The 51 SP-format plugins
(MV_* movies, Pl* hall events, ad objects) also carry an ATTR, but their mID is
a per-file 4cc and their mItemType is 0, so they are not listed here.
**Source:** each plugin's ATTR id 128 and DESC id 128, decoded with
`tools/yoottower/t2p_schema.py` (field order proven in
doc/yoottower/plugin-schema.md §2.1). The C++ table is
`source/ItemTypeCatalog.h` and `testItemTypeCatalog` checks it against every
plugin.

## Correction: which field is which

Earlier versions of this file called the 4cc at ATTR offset 1 `mItemType` and
the integer at offset 21 `mID`. That was backwards (plugin-schema.md §0):
`CFilePlugin::CFilePlugin` (T2DLL 0x1000df20) reads, in order,

| Offset | Field | Type | Meaning |
|---|---|---|---|
| 1 | **mID** | 4cc | the plugin's unique id (`BURG`, `StdE`); save files reference it |
| 5 | mAttrib | u32 | world-availability mask (Tokyo 0x1/0x2/0x4, Kegon 0x10, Hawai 0x100) |
| 9 | mDependID | 4cc | required plugin: `****` none, `FRON` = needs a hotel Front |
| 13 | mGameLevel | i32 | star level that unlocks the item |
| 17 | mSecretItem | i32 | hidden unless secret mode |
| 21 | **mItemType** | i32 | numeric item type, equal to the TNDF/MVDF/OODF mToolType (Floor 1000, Office 1200, Burger 1301, Simple Elevator 1810) |
| 25 | mMustItem | i32 | always-available item |
| 29 | mPluginName | pstr | display name |

Every integer is a 4-byte LE slot. Both columns below are unique across the
population.

## How the 4cc is stored

The 4 bytes sit in the file in reverse ("file order", e.g. `GRUB`). Read as a
little-endian u32 they are the Mac 4cc constant `'BURG'` = 0x42555247, which
is how the engine and plugin-schema.md spell it ("Mac order"). In C++,
`FourCC::str()` gives the file order, `FourCC::macStr()` the Mac order, and
`ItemTypeEntry::pluginId` stores the constant. Case varies (`StdE`, `KElv`).
DefaultToolPalette is the only text-form ATTR; its mID is the plain number
1000 and its mItemType is 1414484044 (`'TOPL'`).

## Catalog (sorted by plugin id)

| mID (Mac) | file bytes | mItemType | Class | mPluginName | File |
|---|---|---|---|---|---|
| 1000 |  | 1414484044 | ToolPalette | Default Tool palette | DefaultToolPalette |
| ALHS | SHLA | 1625 | Tenant | Aloha Shop | Aloha |
| APRT | TRPA | 1101 | Tenant | Condo | Condo |
| BABA | ABAB | 1603 | Tenant | Barber Shop | Barber |
| BCDM | MDCB | 1645 | Tenant | BCD-Mart | BCDMart |
| BigE | EgiB | 1812 | Mover | Large Elevator | BigElev |
| BILL | LLIB | 2100 | OuterObj | Billboard | Billboard |
| BOOK | KOOB | 1601 | Tenant | Book Store | Bookstore |
| BTKP | PKTB | 1723 | Tenant | Boat Deck | Boatdeck |
| BURG | GRUB | 1301 | Tenant | Burger land | Burger |
| BUTQ | QTUB | 1641 | Tenant | Boutique | Boutique |
| CAFE | EFAC | 1300 | Tenant | coffee.com | Coffee |
| CATH | HTAC | 2000 | Tenant | Final | Cathedral |
| CINA | ANIC | 1346 | Tenant | Uncle Chow's | Chinese |
| CINE | ENIC | 1720 | Tenant | Movie Theater | Theater |
| CKTL | LTKC | 1348 | Tenant | Cocktail Lounge | Cocktail |
| CONB | BNOC | 1622 | Tenant | Convenience Store | Convenience |
| COND | DNOC | 1120 | Tenant | Hawaiian Condo | HawaianCondo |
| DANG | GNAD | 1381 | Tenant | Japanese Snacks | J-Snack |
| DFSX | XSFD | 1644 | Tenant | Duty Free Shop | DutyFree |
| DRUG | GURD | 1620 | Tenant | Drug Store | Drug |
| DUST | TSUD | 1542 | Tenant | Trash Facility | TrashFacility |
| ELEC | CELE | 1640 | Tenant | Electronic Store | Electric |
| ESCA | ACSE | 1801 | Mover | Escalators | Escalator |
| EVNT | TNVE | 1722 | Tenant | Event Center | EventCenter |
| FAMI | IMAF | 1323 | Tenant | Jenny's | Jenny's |
| FCOT | TOCF | 1360 | Tenant | Food Court | FoodCoat |
| FLOR | ROLF | 1000 | Tenant | Floor | Floor |
| FLOW | WOLF | 1602 | Tenant | 1-800 FLOWERS | Flower |
| FRCH | HCRF | 1347 | Tenant | Eiffel Tower | French |
| FRON | NORF | 1400 | Tenant | Front Desk | Front |
| FRUT | TURF | 1646 | Tenant | Fruit Stand | Fruits |
| GAME | EMAG | 1660 | Tenant | Gameworks | Gameworks |
| HEND | DNEH | 1541 | Tenant | Power Room | PowerRoom |
| HIHO | OHIH | 2002 | Tenant | Final | Attraction |
| HOSP | PSOH | 1544 | Tenant | Doctor's Office | Doctor's |
| HREN | NERH | 1102 | Tenant | Apartment | Apartment |
| HSGL | LGSH | 1401 | Tenant | Single rooms | Singleroom |
| HSUI | IUSH | 1403 | Tenant | Suite rooms | Suiteroom |
| HTWN | NWTH | 1402 | Tenant | Twin rooms | Twinroom |
| HWAI | IAWH | 20 | World | Hawaii | Hawai |
| ICES | SECI | 1600 | Tenant | Ice Cream Stand | Icecream |
| IKOI | IOKI | 1741 | Tenant | Gathering Area | GatheringArea |
| INDO | ODNI | 1343 | Tenant | Maharaja | Indian |
| ITAL | LATI | 1342 | Tenant | Mama Pizano's | Itarian |
| IZAK | KAZI | 1305 | Tenant | J-Pub | J-Pub |
| JCIG | GICJ | 1604 | Tenant | Cigarette Vending Machine | Cigar |
| JDNK | KNDJ | 1605 | Tenant | Juice Vending Machine | Drink |
| JPNF | FNPJ | 1345 | Tenant | Japanese Cuisine | Japanese |
| KCIG | GICK | 1684 | Tenant | Cigarette Vending Machine | kCigar |
| KDUS | SUDK | 1582 | Tenant | Trash Facility | KTrashFacility |
| KEBI | IBEK | 1521 | Tenant | Security Room | Securityroom |
| KEIS | SIEK | 1380 | Tenant | Old Time Cafe | OldTimeCafe |
| KELV | VLEK | 1818 | Mover | K.F.Simple Elevator | KElev |
| KFLO | OLFK | 1010 | Tenant | Floor | KFloor |
| KGON | NOGK | 10 | World | The Kegon Falls | Kegon |
| KICE | ECIK | 1681 | Tenant | Ice Cream Stand | KIcecream |
| KJUC | CUJK | 1685 | Tenant | Juice Vending Machine | KDrink |
| KOBE | EBOK | 1481 | Tenant | Small Room | Smallroom |
| KSER | RESK | 1819 | Mover | K.F.Service Elevator | KServ |
| KTOI | IOTK | 1581 | Tenant | Restroom | KRestroom |
| MARK | KRAM | 1648 | Tenant | Supermarket | SuperMarket |
| MEDA | ADEM | 1683 | Tenant | Coin Press Machine | Coin |
| MEGA | AGEM | 1721 | Tenant | Classic Movie Theatre | ClassicTheater |
| MENS | SNEM | 1642 | Tenant | Men's Clothing Shop | Men'sClothing |
| MENT | TNEM | 1404 | Tenant | Housekeeping | Maintenance |
| MEXC | CXEM | 1322 | Tenant | Loco Taco | Mexcan |
| MILK | KLIM | 1680 | Tenant | Juice Bar | Juicebar |
| MIYA | AYIM | 1687 | Tenant | Souvenirs | Souvenirs |
| OBEY | YEBO | 1482 | Tenant | Large Room | Largeroom |
| OFFC | CFFO | 1200 | Tenant | Office | Office |
| ORJU | UJRO | 1610 | Tenant | Orange Julius | OrangeJulius |
| PARK | KRAP | 1543 | Tenant | Parking | Parking |
| PETS | STEP | 1621 | Tenant | Pet Shop | Petshop |
| RAMN | NMAR | 1303 | Tenant | Ramen Noodle House | Ramen |
| RCOD | DOCR | 1624 | Tenant | Record store | Record |
| RFRO | ORFR | 1480 | Tenant | Front Desk | RFront |
| RMEN | NEMR | 1483 | Tenant | Housekeeping | RMaintenance |
| SCOL | LOCS | 1740 | Tenant | Elementary School | School |
| SDUM | MUDS | 2001 | Tenant | Final | Stadium |
| SEIN | NIES | 1580 | Tenant | Fire Department | FireDepartment |
| SerE | EreS | 1811 | Mover | Service Elevator | ServElev |
| SFOD | DOFS | 1321 | Tenant | Ahabs | SeaFood |
| SHAS | SAHS | 1682 | Tenant | Photo Studio | Photo |
| SHOK | KOHS | 1382 | Tenant | Traditional Japanese | Trad-Japanese |
| SKYB | BYKS | 1001 | Tenant | Sky Bridge | SkyBrdge |
| SNJK | KJNS | 0 | World | Tokyo | Tokyo |
| SPOR | ROPS | 1643 | Tenant | Sporting Goods | Sportsshop |
| SSOB | BOSS | 1304 | Tenant | Soba Noodle Stand | SobaNoodle |
| STAR | RATS | 1800 | Mover | Stairs | Stair |
| StdE | EdtS | 1810 | Mover | Simple Elevator | StdElev |
| STEK | KETS | 1320 | Tenant | Big Country | Steak |
| SUBW | WBUS | 1701 | Tenant | Subway Station | Subway |
| SURF | FRUS | 1626 | Tenant | Surf Shop | SurfShop |
| SUSI | ISUS | 1341 | Tenant | Sushi-Man | Sushibar |
| TAKI | IKAT | 1920 | Tenant | Kegon Spa | Spa |
| TARE | ERAT | 1688 | Tenant | Kids Store | Kids |
| TENB | BNET | 1901 | Tenant | Observatory | Observatory |
| TICK | KCIT | 1900 | Tenant | Ticket Booth | Ticket |
| TOIL | LIOT | 1500 | Tenant | Restroom | Restroom |
| TOYS | SYOT | 1647 | Tenant | ToyWorld | Toyworld |
| TUKE | EKUT | 1686 | Tenant | Local Pickle Farm | LocalPickle |
| VIDO | ODIV | 1623 | Tenant | Video & DVD | Video&DVD |
| WETO | OTEW | 1351 | Tenant | Mrs.Weinstein'sToffee | Weinstein'sToffee |
| YAKI | IKAY | 1344 | Tenant | Korean B-BQ | KoreanBBQ |

## Class distribution

| Class       | Count | DESC tag (file / Mac) | Examples |
|-------------|-------|-----------------------|----------|
| Tenant      | 93    | lPnT / TnPl           | Apartment, Burger, Office, Condo |
| Mover       | 7     | lPvM / MvPl           | Stairs, Escalator, elevators |
| World       | 3     | lPdW / WdPl           | Hawai, Tokyo, Kegon |
| OuterObj    | 1     | lPoO / OoPl           | Billboard |
| ToolPalette | 1     | LPOT / TOPL           | Default Tool palette |

## Notes

- **Placeholder names:** Cathedral, Stadium and Attraction ship with
  `mPluginName = "Final"`; the file name is the intended identity.
- **K. variants:** KFloor, KElev, KServ and the other `K*` plugins are the
  Kegon scenario variants of standard items.
- **Duplicate display names:** several plugins share an in-game name (three
  "Restroom", two "Floor", two "Front Desk", ...). The plugin id and the
  numeric item type disambiguate them.
- **mItemType vs mToolType:** they are equal for every plugin except Billboard
  (ATTR 2100, OODF 2000).
- The C++ lookups are `lookupPluginId(FourCC)`, `lookupPluginId("BURG")`
  (Mac order, case-insensitive) and `lookupItemType(1301)` in
  `source/ItemTypeCatalog.h`.
