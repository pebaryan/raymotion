# Austria through time — historical notes

The video is a short visual summary. The globe and modern outline use raymotion's
bundled Natural Earth data; the architecture and alpine terrain are symbolic models.

| Scene | Fact used | Source |
| --- | --- | --- |
| Noricum and Rome | Noricum was largely integrated into the Roman Empire around 15 BC; Roman Vindobona preceded Vienna. | [Austrian Foreign Ministry](https://www.bmeia.gv.at/oev-wien/about-austria/history/) |
| Ostarrichi | The name appears in a deed of Otto III in 996. | [Austrian Foreign Ministry](https://www.bmeia.gv.at/oev-wien/about-austria/history/) |
| Habsburg centuries | The Habsburgs were given the Duchy of Austria in 1282; the Austrian Empire began in 1804 and the Dual Monarchy in 1867. | [Austrian Foreign Ministry](https://www.bmeia.gv.at/oev-wien/about-austria/history/), [Austrian citizenship history booklet](https://www.bmeia.gv.at/fileadmin/user_upload/Zentrale/Integration/Staatsbuerger/Stb_Lernunterlage.pdf) |
| First Republic | The republic was proclaimed on 12 November 1918 after World War I and the monarchy's collapse. | [Austrian Parliament](https://www.parlament.gv.at/verstehen/historisches/1918-1945/geburt-der-republik) |
| 1933–45 | Parliamentary democracy ended in 1933/34; Austria was annexed by Nazi Germany in 1938. Persecution and the Holocaust followed, with Austrian participation in Nazi crimes. | [Austrian Parliament](https://www.parlament.gv.at/verstehen/historisches/1918-1945), [Austrian Foreign Ministry](https://www.bmeia.gv.at/fileadmin/user_upload/Zentrale/Integration/Austria_From_the_beginning.pdf) |
| Restoration and State Treaty | The republic was restored on 27 April 1945; the State Treaty was signed on 15 May 1955. | [Austrian Parliament](https://www.parlament.gv.at/kriegsende-staatsvertrag-eu-beitritt/index.html) |
| National Day | Parliament passed the neutrality law on 26 October 1955. The date became National Day in 1965. | [Austrian Parliament](https://www.parlament.gv.at/verstehen/historisches/1945-1995/staatsvertrag) |
| Europe and today | Austria joined the EU in 1995. Population grew from 7,948,278 in 1995 to 9,215,956 on 1 January 2026, rounded in the video to 7.95 and 9.22 million. | [European Commission](https://economy-finance.ec.europa.eu/euro/eu-countries-and-euro/austria-and-euro_en), [Statistics Austria yearbook](https://statistik.at/fileadmin/user_upload/Demographisches-JB-2022_Web_barrierefrei.pdf), [Statistics Austria current population](https://www.statistik.at/en/statistics/population-and-society/population/population-stock/population-at-beginning-of-year/quarter) |
| Alpine landscape | Statistics Austria reports 573 summits at or above 3,000 m in Tyrol. The video renders an illustrative terrain mesh, not measured elevation data. | [Statistics Austria, *Austria: Figures, Data, Facts 2025/26*](https://www.statistik.at/fileadmin/publications/OZDF-EN-25-26.pdf) |

Build and render from the repository root:

```powershell
python tools\make_austria_audio.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_austria_voice.ps1
cmake --build build --target raymotion --config Release
build\raymotion.exe render austria-through-time --jobs 4
```
