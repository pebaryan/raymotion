# Time Travel: Austria — script and sources

`build/raymotion render austria-time-travel` (`examples/showcase/austria_time_travel.cpp`),
about 140 seconds, made for Austria's National Day (26 October 2026).

## Narration

Spoken lines are in `assets/vo/austria-time-travel/lines.json` (`text` is what the voice
says, `caption` what the captions show). The takes were generated with the author's voice
clone (Qwen3-TTS, `tools/vo_batch.py`), then cut clean at the head (`tools/vo_trim_head.py`;
s01, s02 and s05 by hand, just before the first word). The spoken text spells the river
Ybbs as "Eeps", the way it is pronounced; the caption keeps the real spelling.

| # | Chapter | Year on the dial | Line |
|---|---|---|---|
| 1 | Today | 2026 | Today is 26 October: Austria's National Day. Set the dial back. Five thousand years. Hold on. |
| 2 | The ice | 3300 BC → 1991 | Around 3300 BC, a man climbs into the Ötztal Alps and never comes down. In 1991, hikers find him in the ice, his copper axe still at his side. |
| 3 | Salt | 1400 BC → 800 BC | Salt made these mountains rich. Miners were digging above Hallstatt more than 3,000 years ago, and the village gave its name to an age of Europe. |
| 4 | Rome | 15 BC → AD 100 | Then Rome came north. The kingdom of Noricum became a Roman province, and the Danube became the frontier. A legion's camp on its bank was called Vindobona. Today, we call it Vienna. |
| 5 | The name | 996 | In 996, an emperor's deed grants land by the river Ybbs, in a region the people call Ostarrîchi. It is the oldest written trace of Austria's name. |
| 6 | Empire | 1278 → 1683 → 1910 | The Habsburgs take Austria in the 1200s and hold it for more than six centuries. Vienna survives the Ottoman siege of 1683. By 1910, 51 million people live under the crown of Austria-Hungary. |
| 7 | Collapse | 1914 → 1918 | Four years of world war break it apart. In November 1918, the empire is gone, and a small republic is proclaimed in Vienna. |
| 8 | 1938–1945 | 1938 → 1945 | Democracy does not last. In March 1938, Nazi Germany annexes Austria. More than 66,000 Austrian Jews are murdered in the Holocaust. Each light is one of them. |
| 9 | Free | 1945 → 1955 | In April 1945, the Second Republic is declared. Four powers occupy the country for ten years, until the State Treaty of May 1955. The last soldiers leave in October. |
| 10 | Neutrality | 1955 → 1965 | On 26 October 1955, parliament declares Austria permanently neutral. Ten years later, that day becomes the National Day. |
| 11 | Now | 1995 → 2026 | In 1995, Austria joins the European Union. A country of 4.5 million in 1869 is 9.2 million today. |
| 12 | Home | 2026 | Five thousand years, and we are home. Happy National Day, Austria. |

## Facts and sources

- **Ötzi.** Radiocarbon dates for the Iceman and his equipment cluster in 3360–3100 BC
  (Radiocarbon, "AMS 14C dating of equipment from the Iceman and of spruce logs from the
  prehistoric salt mines of Hallstatt",
  <https://www.cambridge.org/core/product/C202D5BD00F1F56399367B0FD7979CA9>). Found on
  19 September 1991 at the Tisenjoch (3,210 m) on today's Austrian–Italian border; his axe
  has a yew haft and a copper blade (South Tyrol Museum of Archaeology).
- **Hallstatt.** Salt was mined there from the 15th century BC (Natural History Museum
  Vienna, <https://www.nhm-wien.ac.at/hallstatt/en/salt_mine/bronze_age>); the Hallstatt
  period of European prehistory is roughly 800–450 BC.
- **Noricum, Vindobona.** Noricum came under Rome around 15 BC and later became a
  province; the Danube was the frontier (limes), with legionary fortresses at Vindobona
  (Vienna), Carnuntum and Lauriacum (Enns). Virunum was Noricum's capital.
- **Ostarrîchi.** Deed of Emperor Otto III, Bruchsal, 1 November 996, granting the
  bishopric of Freising land round Neuhofen an der Ybbs "in regione vulgari vocabulo
  ostarrichi"; held by the Bavarian State Archives, Munich (Land Niederösterreich,
  <https://noe.gv.at/noe/Geschichte-Landeskunde/Ostarrichi-Urkunde.html>).
- **Habsburgs, 1683, 1910.** Rudolf I enfeoffed his sons with Austria in 1282; the second
  Ottoman siege of Vienna was lifted on 12 September 1683. The census of 31 December 1910
  counted 51,390,223 people in Austria-Hungary, including Bosnia and Herzegovina
  (<https://en.wikipedia.org/wiki/Ethnic_and_religious_composition_of_Austria-Hungary>,
  <https://ww1.habsburger.net/en/node/5956>). The map extrudes only today's states that lay
  wholly within the monarchy; it also held parts of today's Poland, Ukraine, Romania, Italy,
  Serbia and Montenegro.
- **1918.** The Republic of German-Austria was proclaimed on 12 November 1918.
- **1938–1945.** Annexation by Nazi Germany in March 1938. The DÖW (Documentation Centre
  of Austrian Resistance) counts more than 66,000 Austrian Jews murdered in the Shoah and has
  identified 63,863 of them by name
  (<https://www.doew.at/cms/download/5kmg2/Lists of Annihilation.pdf>). The 1938–1945
  chapter draws 66,000 points inside Austria's outline.
- **1945–1955.** The Second Republic was declared on 27 April 1945; the State Treaty was
  signed at the Belvedere on 15 May 1955 and came into force on 27 July 1955; the last
  occupation troops were to leave by 25 October 1955.
- **26 October.** Parliament passed the constitutional law on permanent neutrality on
  26 October 1955; Federal Act No. 298 of 25 October 1965 made it the National Day
  (Austrian Parliament, <https://www.parlament.gv.at/en/experience/events/1285635>;
  <https://www.archive.austria.org/national-day>).
- **Population.** Censuses of today's territory: 4,497,880 (1869), 6,648,310 (1910),
  6,760,044 (1934), 6,933,905 (1951), 8,032,587 (2001), 8,969,068 (2021)
  (<https://en.wikipedia.org/wiki/Demographics_of_Austria>); 9,219,113 on 1 January 2026,
  preliminary (Statistik Austria,
  <https://www.statistik.at/fileadmin/announcement/2026/02/20260209Bevoelkerung1.1.2026EN.pdf>).
- **EU.** Austria joined the European Union on 1 January 1995.

## Music

Generated by `genScore` from fragments of Austrian composers' music, all in the public
domain: Johann Strauss II, *An der schönen blauen Donau* (1866) — the waltz theme on a music
box, rewound, in the minor as the empire falls, and in full at the end; W. A. Mozart,
*Eine kleine Nachtmusik* K. 525 (1787) for the Habsburg court and the neutrality fanfare,
and the *Rondo alla Turca* from the Sonata K. 331 (1783) under the siege of 1683. The
1938–1945 chapter has no melody.
