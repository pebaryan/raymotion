// icons.cpp -- social media icons (Simple Icons paths, CC0) and the raymotion logo, with
// their prefabs.
#include "rmtn/icons.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/media.hpp"
#include "rmtn/prefab.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace rmtn {
namespace icons {

namespace {

struct Brand {
    const char* name;
    const char* title;
    unsigned color;
    bool dark;              // too dark to show on a dark frame: Glyph uses Look::color
    const char* d;          // Simple Icons path data, 24 x 24
    int under;              // contour filled white beneath the Glyph look (-1: none)
    int badgeOnly;          // the contour alone on a badge, scaled to `badgeSize` (-1: the whole mark)
    float badgeSize;        // the badge mark's larger side, as a share of the badge
};

const Brand BRANDS[] = {
    { "x", "X", 0x000000, true,
      "M18.901 1.153h3.68l-8.04 9.19L24 22.846h-7.406l-5.8-7.584-6.638 7.584H.474l8.6-9.83L0 1.154h7.594l5.243 6.932Z"
      "M17.61 20.644h2.039L6.486 3.24H4.298Z",
      -1, -1, 0.5f },
    { "youtube", "YouTube", 0xFF0000, false,
      "M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 "
      ".502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 "
      "9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12"
      "l-6.273 3.568z",
      1, 1, 0.36f },
    { "github", "GitHub", 0x181717, true,
      "M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04"
      "-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 "
      "1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 "
      "0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 "
      "2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61"
      "-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 "
      "24 17.592 24 12.297c0-6.627-5.373-12-12-12",
      -1, -1, 0.6f },
    { "instagram", "Instagram", 0xE4405F, false,
      "M7.0301.084c-1.2768.0602-2.1487.264-2.911.5634-.7888.3075-1.4575.72-2.1228 1.3877-.6652.6677-1.075 1.3368-1.3802 "
      "2.127-.2954.7638-.4956 1.6365-.552 2.914-.0564 1.2775-.0689 1.6882-.0626 4.947.0062 3.2586.0206 3.6671.0825 "
      "4.9473.061 1.2765.264 2.1482.5635 2.9107.308.7889.72 1.4573 1.388 2.1228.6679.6655 1.3365 1.0743 2.1285 1.38.7632"
      ".295 1.6361.4961 2.9134.552 1.2773.056 1.6884.069 4.9462.0627 3.2578-.0062 3.668-.0207 4.9478-.0814 1.28-.0607 "
      "2.147-.2652 2.9098-.5633.7889-.3086 1.4578-.72 2.1228-1.3881.665-.6682 1.0745-1.3378 1.3795-2.1284.2957-.7632"
      ".4966-1.636.552-2.9124.056-1.2809.0692-1.6898.063-4.948-.0063-3.2583-.021-3.6668-.0817-4.9465-.0607-1.2797-.264"
      "-2.1487-.5633-2.9117-.3084-.7889-.72-1.4568-1.3876-2.1228C21.2982 1.33 20.628.9208 19.8378.6165 19.074.321 "
      "18.2017.1197 16.9244.0645 15.6471.0093 15.236-.005 11.977.0014 8.718.0076 8.31.0215 7.0301.0839m.1402 21.6932"
      "c-1.17-.0509-1.8053-.2453-2.2287-.408-.5606-.216-.96-.4771-1.3819-.895-.422-.4178-.6811-.8186-.9-1.378-.1644"
      "-.4234-.3624-1.058-.4171-2.228-.0595-1.2645-.072-1.6442-.079-4.848-.007-3.2037.0053-3.583.0607-4.848.05-1.169"
      ".2456-1.805.408-2.2282.216-.5613.4762-.96.895-1.3816.4188-.4217.8184-.6814 1.3783-.9003.423-.1651 1.0575-.3614 "
      "2.227-.4171 1.2655-.06 1.6447-.072 4.848-.079 3.2033-.007 3.5835.005 4.8495.0608 1.169.0508 1.8053.2445 2.228"
      ".408.5608.216.96.4754 1.3816.895.4217.4194.6816.8176.9005 1.3787.1653.4217.3617 1.056.4169 2.2263.0602 1.2655"
      ".0739 1.645.0796 4.848.0058 3.203-.0055 3.5834-.061 4.848-.051 1.17-.245 1.8055-.408 2.2294-.216.5604-.4763.96"
      "-.8954 1.3814-.419.4215-.8181.6811-1.3783.9-.4224.1649-1.0577.3617-2.2262.4174-1.2656.0595-1.6448.072-4.8493"
      ".079-3.2045.007-3.5825-.006-4.848-.0608M16.953 5.5864A1.44 1.44 0 1 0 18.39 4.144a1.44 1.44 0 0 0-1.437 1.4424"
      "M5.8385 12.012c.0067 3.4032 2.7706 6.1557 6.173 6.1493 3.4026-.0065 6.157-2.7701 6.1506-6.1733-.0065-3.4032"
      "-2.771-6.1565-6.174-6.1498-3.403.0067-6.156 2.771-6.1496 6.1738M8 12.0077a4 4 0 1 1 4.008 3.9921A3.9996 3.9996 "
      "0 0 1 8 12.0077",
      -1, -1, 0.58f },
    { "tiktok", "TikTok", 0x000000, true,
      "M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05"
      "-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-.93-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 "
      "3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 "
      "2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 "
      "1.75-.21.51-.15 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79"
      ".06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z",
      -1, -1, 0.54f },
    { "whatsapp", "WhatsApp", 0x25D366, false,
      "M17.472 14.382c-.297-.149-1.758-.867-2.03-.967-.273-.099-.471-.148-.67.15-.197.297-.767.966-.94 1.164-.173.199"
      "-.347.223-.644.075-.297-.15-1.255-.463-2.39-1.475-.883-.788-1.48-1.761-1.653-2.059-.173-.297-.018-.458.13-.606"
      ".134-.133.298-.347.446-.52.149-.174.198-.298.298-.497.099-.198.05-.371-.025-.52-.075-.149-.669-1.612-.916-2.207"
      "-.242-.579-.487-.5-.669-.51-.173-.008-.371-.01-.57-.01-.198 0-.52.074-.792.372-.272.297-1.04 1.016-1.04 2.479 0 "
      "1.462 1.065 2.875 1.213 3.074.149.198 2.096 3.2 5.077 4.487.709.306 1.262.489 1.694.625.712.227 1.36.195 1.871"
      ".118.571-.085 1.758-.719 2.006-1.413.248-.694.248-1.289.173-1.413-.074-.124-.272-.198-.57-.347m-5.421 7.403h"
      "-.004a9.87 9.87 0 01-5.031-1.378l-.361-.214-3.741.982.998-3.648-.235-.374a9.86 9.86 0 01-1.51-5.26c.001-5.45 "
      "4.436-9.884 9.888-9.884 2.64 0 5.122 1.03 6.988 2.898a9.825 9.825 0 012.893 6.994c-.003 5.45-4.437 9.884-9.885 "
      "9.884m8.413-18.297A11.815 11.815 0 0012.05 0C5.495 0 .16 5.335.157 11.892c0 2.096.547 4.142 1.588 5.945L.057 24"
      "l6.305-1.654a11.882 11.882 0 005.683 1.448h.005c6.554 0 11.89-5.335 11.893-11.893a11.821 11.821 0 00-3.48-8.413Z",
      -1, -1, 0.6f },
    { "telegram", "Telegram", 0x26A5E4, false,
      "M11.944 0A12 12 0 0 0 0 12a12 12 0 0 0 12 12 12 12 0 0 0 12-12A12 12 0 0 0 12 0a12 12 0 0 0-.056 0z"
      "m4.962 7.224c.1-.002.321.023.465.14a.506.506 0 0 1 .171.325c.016.093.036.306.02.472-.18 1.898-.962 6.502-1.36 "
      "8.627-.168.9-.499 1.201-.82 1.23-.696.065-1.225-.46-1.9-.902-1.056-.693-1.653-1.124-2.678-1.8-1.185-.78-.417"
      "-1.21.258-1.91.177-.184 3.247-2.977 3.307-3.23.007-.032.014-.15-.056-.212s-.174-.041-.249-.024c-.106.024-1.793 "
      "1.14-5.061 3.345-.48.33-.913.49-1.302.48-.428-.008-1.252-.241-1.865-.44-.752-.245-1.349-.374-1.297-.789.027-.216"
      ".325-.437.893-.663 3.498-1.524 5.83-2.529 6.998-3.014 3.332-1.386 4.025-1.627 4.476-1.635z",
      1, 1, 0.56f },
    { "twitch", "Twitch", 0x9146FF, false,
      "M4.285 0 2.143 5.714v15.429h5.714V24l2.857-2.857H15l7.857-7.857V0H4.285z"
      "m16.429 12.286-4.286 4.286h-4.286l-3.571 3.571v-3.571H4.286V2.143h16.428v10.143z"
      "M17.143 5.714H15v6.429h2.143V5.714zM11.429 5.714H9.286v6.429h2.143V5.714z",
      -1, -1, 0.6f },
    { "steam", "Steam", 0x000000, true,
      "M11.979 0C5.678 0 .511 4.86.022 11.037l6.432 2.658c.545-.371 1.203-.59 1.912-.59.063 0 .125.004.188.006l2.861-4.142"
      "V8.91c0-2.495 2.028-4.524 4.524-4.524 2.494 0 4.524 2.031 4.524 4.527s-2.03 4.525-4.524 4.525h-.105l-4.076 2.911"
      "c0 .052.004.105.004.159 0 1.875-1.515 3.396-3.39 3.396-1.635 0-3.016-1.173-3.331-2.727L.436 15.27C1.862 20.307"
      " 6.486 24 11.979 24c6.627 0 11.999-5.373 11.999-12S18.605 0 11.979 0zM7.54 18.21l-1.473-.61c.262.543.714.999"
      " 1.314 1.25 1.297.539 2.793-.076 3.332-1.375.263-.63.264-1.319.005-1.949s-.75-1.121-1.377-1.383"
      "c-.624-.26-1.29-.249-1.878-.03l1.523.63c.956.4 1.409 1.5 1.009 2.455-.397.957-1.497 1.41-2.454 1.012H7.54z"
      "m11.415-9.303c0-1.662-1.353-3.015-3.015-3.015-1.665 0-3.015 1.353-3.015 3.015 0 1.665 1.35 3.015 3.015 3.015"
      " 1.663 0 3.015-1.35 3.015-3.015zm-5.273-.005c0-1.252 1.013-2.266 2.265-2.266 1.249 0 2.266 1.014 2.266 2.266"
      " 0 1.251-1.017 2.265-2.266 2.265-1.253 0-2.265-1.014-2.265-2.265z",
      -1, -1, 0.6f },
    { "itchdotio", "Itch.io", 0xFA5C5C, false,
      "M3.13 1.338C2.08 1.96.02 4.328 0 4.95v1.03c0 1.303 1.22 2.45 2.325 2.45 1.33 0 2.436-1.102 2.436-2.41 0 1.308 "
      "1.07 2.41 2.4 2.41 1.328 0 2.362-1.102 2.362-2.41 0 1.308 1.137 2.41 2.466 2.41h.024c1.33 0 2.466-1.102 2.466-"
      "2.41 0 1.308 1.034 2.41 2.363 2.41 1.33 0 2.4-1.102 2.4-2.41 0 1.308 1.106 2.41 2.435 2.41C22.78 8.43 24 7.282"
      " 24 5.98V4.95c-.02-.62-2.082-2.99-3.13-3.612-3.253-.114-5.508-.134-8.87-.133-3.362 0-7.945.053-8.87.133zm6.376"
      " 6.477a2.74 2.74 0 0 1-.468.602c-.5.49-1.19.795-1.947.795a2.786 2.786 0 0 1-1.95-.795c-.182-.178-.32-.37-.446-"
      ".59-.127.222-.303.412-.486.59a2.788 2.788 0 0 1-1.95.795c-.092 0-.187-.025-.264-.052-.107 1.113-.152 2.176-.16"
      "8 2.95v.005l-.006 1.167c.02 2.334-.23 7.564 1.03 8.85 1.952.454 5.545.662 9.15.663 3.605 0 7.198-.21 9.15-.664"
      " 1.26-1.284 1.01-6.514 1.03-8.848l-.006-1.167v-.004c-.016-.775-.06-1.838-.168-2.95-.077.026-.172.052-.263.052a"
      "2.788 2.788 0 0 1-1.95-.795c-.184-.178-.36-.368-.486-.59-.127.22-.265.412-.447.59a2.786 2.786 0 0 1-1.95.794c-"
      ".76 0-1.446-.303-1.948-.793a2.74 2.74 0 0 1-.468-.602 2.738 2.738 0 0 1-.463.602 2.787 2.787 0 0 1-1.95.794h-."
      "16a2.787 2.787 0 0 1-1.95-.793 2.738 2.738 0 0 1-.464-.602zm-2.004 2.59v.002c.795.002 1.5 0 2.373.953.687-.072"
      " 1.406-.108 2.125-.107.72 0 1.438.035 2.125.107.873-.953 1.578-.95 2.372-.953.376 0 1.876 0 2.92 2.934l1.123 4"
      ".028c.832 2.995-.266 3.068-1.636 3.07-2.03-.075-3.156-1.55-3.156-3.025-1.124.184-2.436.276-3.748.277-1.312 0-2"
      ".624-.093-3.748-.277 0 1.475-1.125 2.95-3.156 3.026-1.37-.004-2.468-.077-1.636-3.072l1.122-4.027c1.045-2.934 2"
      ".545-2.934 2.92-2.934zM12 12.714c-.002.002-2.14 1.964-2.523 2.662l1.4-.056v1.22c0 .056.56.033 1.123.007.562.02"
      "6 1.124.05 1.124-.008v-1.22l1.4.055C14.138 14.677 12 12.713 12 12.713z",
      -1, -1, 0.6f },
    // Marketplace source paths: Simple Icons develop, except Amazon from 12.4.0.
    { "shopee", "Shopee", 0xEE4D2D, false,
      "M15.9414 17.9633c.229-1.879-.981-3.077-4.1758-4.0969-1.548-.528-2.277-1.22-2.26-2.1719.065-1.056 1.048-1.825 2"
      ".352-1.85a5.2898 5.2898 0 0 1 2.8838.89c.116.072.197.06.263-.039.09-.145.315-.494.39-.62.051-.081.061-.187-.06"
      "8-.281-.185-.1369-.704-.4149-.983-.5319a6.4697 6.4697 0 0 0-2.5118-.514c-1.909.008-3.4129 1.215-3.5389 2.826-."
      "082 1.1629.494 2.1078 1.73 2.8278.262.152 1.6799.716 2.2438.892 1.774.552 2.695 1.5419 2.478 2.6969-.197 1.047"
      "-1.299 1.7239-2.818 1.7439-1.2039-.046-2.2878-.537-3.1278-1.19l-.141-.11c-.104-.08-.218-.075-.287.03-.05.077-."
      "376.547-.458.67-.077.108-.035.168.045.234.35.293.817.613 1.134.775a6.7097 6.7097 0 0 0 2.8289.727 4.9048 4.904"
      "8 0 0 0 2.0759-.354c1.095-.465 1.8029-1.394 1.9449-2.554zM11.9986 1.4009c-2.068 0-3.7539 1.95-3.8329 4.3899h7."
      "6657c-.08-2.44-1.765-4.3899-3.8328-4.3899zm7.8516 22.5981-.08.001-15.7843-.002c-1.074-.04-1.863-.91-1.971-1.99"
      "1l-.01-.195L1.298 6.2858a.459.459 0 0 1 .45-.494h4.9748C6.8448 2.568 9.1607 0 11.9996 0c2.8388 0 5.1537 2.5689"
      " 5.2757 5.7898h4.9678a.459.459 0 0 1 .458.483l-.773 15.5883-.007.131c-.094 1.094-.979 1.9769-2.0709 2.0059z",
      -1, -1, 0.58f },
    { "amazon", "Amazon", 0xFF9900, false,
      "M.045 18.02c.072-.116.187-.124.348-.022 3.636 2.11 7.594 3.166 11.87 3.166 2.852 0 5.668-.533 8.447-1.595l.315"
      "-.14c.138-.06.234-.1.293-.13.226-.088.39-.046.525.13.12.174.09.336-.12.48-.256.19-.6.41-1.006.654-1.244.743-2."
      "64 1.316-4.185 1.726a17.617 17.617 0 01-10.951-.577 17.88 17.88 0 01-5.43-3.35c-.1-.074-.151-.15-.151-.22 0-.0"
      "47.021-.09.051-.13zm6.565-6.218c0-1.005.247-1.863.743-2.577.495-.71 1.17-1.25 2.04-1.615.796-.335 1.756-.575 2"
      ".912-.72.39-.046 1.033-.103 1.92-.174v-.37c0-.93-.105-1.558-.3-1.875-.302-.43-.78-.65-1.44-.65h-.182c-.48.046-"
      ".896.196-1.246.46-.35.27-.575.63-.675 1.096-.06.3-.206.465-.435.51l-2.52-.315c-.248-.06-.372-.18-.372-.39 0-.0"
      "46.007-.09.022-.15.247-1.29.855-2.25 1.82-2.88.976-.616 2.1-.975 3.39-1.05h.54c1.65 0 2.957.434 3.888 1.29.135"
      ".15.27.3.405.48.12.165.224.314.283.45.075.134.15.33.195.57.06.254.105.42.135.51.03.104.062.3.076.615.01.313.02"
      ".493.02.553v5.28c0 .376.06.72.165 1.036.105.313.21.54.315.674l.51.674c.09.136.136.256.136.36 0 .12-.06.226-.18"
      ".314-1.2 1.05-1.86 1.62-1.963 1.71-.165.135-.375.15-.63.045a6.062 6.062 0 01-.526-.496l-.31-.347a9.391 9.391 0"
      " 01-.317-.42l-.3-.435c-.81.886-1.603 1.44-2.4 1.665-.494.15-1.093.227-1.83.227-1.11 0-2.04-.343-2.76-1.034-.72"
      "-.69-1.08-1.665-1.08-2.94l-.05-.076zm3.753-.438c0 .566.14 1.02.425 1.364.285.34.675.512 1.155.512.045 0 .106-."
      "007.195-.02.09-.016.134-.023.166-.023.614-.16 1.08-.553 1.424-1.178.165-.28.285-.58.36-.91.09-.32.12-.59.135-."
      "8.015-.195.015-.54.015-1.005v-.54c-.84 0-1.484.06-1.92.18-1.275.36-1.92 1.17-1.92 2.43l-.035-.02zm9.162 7.027c"
      ".03-.06.075-.11.132-.17.362-.243.714-.41 1.05-.5a8.094 8.094 0 011.612-.24c.14-.012.28 0 .41.03.65.06 1.05.168"
      " 1.172.33.063.09.099.228.099.39v.15c0 .51-.149 1.11-.424 1.8-.278.69-.664 1.248-1.156 1.68-.073.06-.14.09-.197"
      ".09-.03 0-.06 0-.09-.012-.09-.044-.107-.12-.064-.24.54-1.26.806-2.143.806-2.64 0-.15-.03-.27-.087-.344-.145-.1"
      "66-.55-.257-1.224-.257-.243 0-.533.016-.87.046-.363.045-.7.09-1 .135-.09 0-.148-.014-.18-.044-.03-.03-.036-.04"
      "7-.02-.077 0-.017.006-.03.02-.063v-.06z",
      -1, -1, 0.63f },
    { "ebay", "eBay", 0xE53238, false,
      "M6.056 12.132v-4.92h1.2v3.026c.59-.703 1.402-.906 2.202-.906 1.34 0 2.828.904 2.828 2.855 0 .233-.015.457-.06."
      "668.24-.953 1.274-1.305 2.896-1.344.51-.018 1.095-.018 1.56-.018v-.135c0-.885-.556-1.244-1.53-1.244-.72 0-1.24"
      "5.3-1.305.81h-1.275c.136-1.29 1.5-1.62 2.686-1.62 1.064 0 1.995.27 2.415 1.02l-.436-.84h1.41l2.055 4.125 2.055"
      "-4.126H24l-3.72 7.305h-1.346l1.07-2.04-2.33-4.38c.13.255.2.555.2.93v2.46c0 .346.01.69.04 1.005H16.8a6.543 6.54"
      "3 0 01-.046-.765c-.603.734-1.32.96-2.32.96-1.48 0-2.272-.78-2.272-1.695 0-.15.015-.284.037-.405-.3 1.246-1.36 "
      "2.086-2.767 2.086-.87 0-1.694-.315-2.2-.93 0 .24-.015.494-.04.734h-1.18c.02-.39.04-.855.04-1.245v-1.05h-4.83c."
      "065 1.095.818 1.74 1.853 1.74.718 0 1.355-.3 1.568-.93h1.24c-.24 1.29-1.61 1.725-2.79 1.725C.95 15.009 0 13.82"
      "2 0 12.232c0-1.754.982-2.91 3.116-2.91 1.688 0 2.93.886 2.94 2.806v.005zm9.137.183c-1.095.034-1.77.233-1.77.95"
      " 0 .465.36.97 1.305.97 1.26 0 1.935-.69 1.935-1.814v-.13c-.45 0-.99.006-1.484.022h.012zm-6.06 1.875c1.11 0 1.8"
      "76-.806 1.876-2.02s-.768-2.02-1.893-2.02c-1.11 0-1.89.806-1.89 2.02s.765 2.02 1.875 2.02h.03zm-4.35-2.514c-.04"
      "4-1.125-.854-1.546-1.725-1.546-.944 0-1.694.474-1.815 1.546z",
      -1, -1, 0.68f },
    { "vinted", "Vinted", 0x007782, false,
      "M19.316 0c-.258 0-.571.217-1.415.953-.3.108-.627.027-1.008.613-2.15 3.09-3.825 14.648-5.255 17.984-.286-1.444-"
      ".885-10.837-1.116-13.41-.028-.477.027-1.076.027-1.43 0-2.368-.516-3.567-2.886-3.567-1.198 0-2.382.436-3.008 1."
      "226-.299.408-.409.708-.409 1.443 0 4.915 1.171 12.973 2.478 18.228C7.132 23.688 8.603 24 9.99 24c.654 0 1.307-"
      ".081 2.233-.544 3.212-1.567 4.07-5.84 4.9-9.993.15-.749.899-4.37 1.253-6.275.476-2.6 1.02-5.54 1.347-6.617C19."
      "833.245 19.63 0 19.317 0z",
      -1, -1, 0.60f },
    // Tokopedia's owl/bag is a compact monochrome badge adaptation.
    { "tokopedia", "Tokopedia", 0x03AC0E, false,
      "M4 5h3C7.5 2.5 9.3 1 12 1s4.5 1.5 5 4h3v11.3C20 20.4 17.2 23 13.8 23H4V5z"
      "M9.3 5h5.4C14.3 3.8 13.3 3 12 3S9.7 3.8 9.3 5z"
      "M9 9.2a3.15 3.15 0 1 0 0 6.3 3.15 3.15 0 0 0 0-6.3z"
      "M15 9.2a3.15 3.15 0 1 0 0 6.3 3.15 3.15 0 0 0 0-6.3z"
      "M9 11.2a1.15 1.15 0 1 1 0 2.3 1.15 1.15 0 0 1 0-2.3z"
      "M15 11.2a1.15 1.15 0 1 1 0 2.3 1.15 1.15 0 0 1 0-2.3z"
      "M10.7 17l1.3 1.5 1.3-1.5z",
      -1, -1, 0.63f },
    // Xbox and Nintendo Switch use Simple Icons 12.4.0 paths; both were retired upstream.
    { "xbox", "Xbox", 0x107C10, false,
      "M4.102 21.033C6.211 22.881 8.977 24 12 24c3.026 0 5.789-1.119 7.902-2.967 1.877-1.912-4.316-8.709-7.902-11.417"
      "-3.582 2.708-9.779 9.505-7.898 11.417zm11.16-14.406c2.5 2.961 7.484 10.313 6.076 12.912C23.002 17.48 24 14.861"
      " 24 12.004c0-3.34-1.365-6.362-3.57-8.536 0 0-.027-.022-.082-.042-.063-.022-.152-.045-.281-.045-.592 0-1.985.43"
      "4-4.805 3.246zM3.654 3.426c-.057.02-.082.041-.086.042C1.365 5.642 0 8.664 0 12.004c0 2.854.998 5.473 2.661 7.5"
      "33-1.401-2.605 3.579-9.951 6.08-12.91-2.82-2.813-4.216-3.245-4.806-3.245-.131 0-.223.021-.281.046v-.002zM12 3."
      "551S9.055 1.828 6.755 1.746c-.903-.033-1.454.295-1.521.339C7.379.646 9.659 0 11.984 0H12c2.334 0 4.605.646 6.7"
      "66 2.085-.068-.046-.615-.372-1.52-.339C14.946 1.828 12 3.545 12 3.545v.006z",
      -1, -1, 0.6f },
    { "playstation", "PlayStation", 0x0070D1, false,
      "M8.984 2.596v17.547l3.915 1.261V6.688c0-.69.304-1.151.794-.991.636.18.76.814.76 1.505v5.875c2.441 1.193 4.362-"
      ".002 4.362-3.152 0-3.237-1.126-4.675-4.438-5.827-1.307-.448-3.728-1.186-5.39-1.502zm4.656 16.241l6.296-2.275c."
      "715-.258.826-.625.246-.818-.586-.192-1.637-.139-2.357.123l-4.205 1.5V14.98l.24-.085s1.201-.42 2.913-.615c1.696"
      "-.18 3.785.03 5.437.661 1.848.601 2.04 1.472 1.576 2.072-.465.6-1.622 1.036-1.622 1.036l-8.544 3.107V18.86zM1."
      "807 18.6c-1.9-.545-2.214-1.668-1.352-2.32.801-.586 2.16-1.052 2.16-1.052l5.615-2.013v2.313L4.205 17c-.705.271-"
      ".825.632-.239.826.586.195 1.637.15 2.343-.12L8.247 17v2.074c-.12.03-.256.044-.39.073-1.939.331-3.996.196-6.038"
      "-.479z",
      -1, -1, 0.6f },
    { "nintendoswitch", "Nintendo Switch", 0xE60012, false,
      "M14.176 24h3.674c3.376 0 6.15-2.774 6.15-6.15V6.15C24 2.775 21.226 0 17.85 0H14.1c-.074 0-.15.074-.15.15v23.7c"
      "-.001.076.075.15.226.15zm4.574-13.199c1.351 0 2.399 1.125 2.399 2.398 0 1.352-1.125 2.4-2.399 2.4-1.35 0-2.4-1"
      ".049-2.4-2.4-.075-1.349 1.05-2.398 2.4-2.398zM11.4 0H6.15C2.775 0 0 2.775 0 6.15v11.7C0 21.226 2.775 24 6.15 2"
      "4h5.25c.074 0 .15-.074.15-.149V.15c.001-.076-.075-.15-.15-.15zM9.676 22.051H6.15c-2.326 0-4.201-1.875-4.201-4."
      "201V6.15c0-2.326 1.875-4.201 4.201-4.201H9.6l.076 20.102zM3.75 7.199c0 1.275.975 2.25 2.25 2.25s2.25-.975 2.25"
      "-2.25c0-1.273-.975-2.25-2.25-2.25s-2.25.977-2.25 2.25z",
      -1, -1, 0.6f },
};

#include "language_brands.inc"

struct Prepared {
    const Brand* brand;
    vg::Path glyph;         // centred on the origin
    vg::Path under;         // the contour shown white beneath the Glyph look
    vg::Path badgeMark;     // what a badge shows, centred and scaled to its 24 box
};

vg::Path contour(const vg::Path& p, int i) {
    vg::Path out;
    if (i >= 0 && i < (int)p.contours.size()) out.contours.push_back(p.contours[i]);
    return out;
}

const Prepared& prepared(const std::string& name) {
    static const std::map<std::string, Prepared> all = [] {
        std::map<std::string, Prepared> m;
        for (const Brand& b : BRANDS) {
            Prepared p;
            p.brand = &b;
            p.glyph = vg::Path::parse(b.d).translated({ -12, -12 });
            p.under = contour(p.glyph, b.under);
            if (b.badgeOnly < 0) {
                p.badgeMark = p.glyph;
            } else {
                // Recentre the lone contour on its bounds and scale its larger side to 24.
                vg::Path one = contour(p.glyph, b.badgeOnly);
                const Rectangle r = one.bounds();
                const float k = 24 / std::max(r.width, r.height);
                p.badgeMark = one.translated({ -(r.x + r.width / 2), -(r.y + r.height / 2) }).scaled(k, k);
            }
            m[b.name] = std::move(p);
        }
        for (const Brand& b : LANGUAGES) {
            Prepared p;
            p.brand = &b;
            vg::Path path = vg::Path::parse(b.d);
            if (std::string(b.name) == "java") path = path.scaled(24.f / 128.f, 24.f / 128.f);
            const Rectangle bounds = path.bounds();
            const float k = 24.f / std::max(bounds.width, bounds.height);
            p.glyph = path.translated({ -(bounds.x + bounds.width / 2), -(bounds.y + bounds.height / 2) }).scaled(k, k);
            p.badgeMark = p.glyph;
            m[b.name] = std::move(p);
        }
        return m;
    }();
    auto it = all.find(name);
    if (it == all.end()) throw std::runtime_error("icons: no brand named '" + name + "'");
    return it->second;
}

Vector2 polar(float deg, float r) { return { r * std::cos(deg * (float)PI / 180), r * std::sin(deg * (float)PI / 180) }; }

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
float smooth(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }

// A context whose units are `scale` px, centred at `centre`.
Ctx at(const Ctx& c, Vector2 centre, float scale) {
    Ctx k = c;
    k.o = mapP(c, centre);
    k.s *= scale;
    return k;
}

// The mark itself: filled, or written on while u < 1.
void mark(const Ctx& c, const vg::Path& p, Color col, float u) {
    if (u >= 1) { vg::fill(c, p, col, vg::FillRule::EvenOdd); return; }
    if (u <= 0) return;
    vg::Style s = vg::Style::filled(col);
    s.rule = vg::FillRule::EvenOdd;
    s.stroke = vg::Stroke(col, 0.45f);
    vg::write(c, p, s, u);
}

// TikTok's mark: cyan and red copies offset behind the main one.
void tiktok(const Ctx& c, const vg::Path& p, Color col, float u) {
    const float o = 0.55f;
    mark(c, p.translated({ -o, -o }), mkColor(0x25F4EE), u);
    mark(c, p.translated({ o, o }), mkColor(0xFE2C55), u);
    mark(c, p, col, u);
}

// Half the shape's width at height y (the shape is `size` across, centred).
float extent(Shape shape, float size, float corner, float y) {
    const float half = size / 2, ay = std::fabs(y);
    if (ay > half) return 0;
    if (shape == Shape::Square) return half;
    if (shape == Shape::Circle) return std::sqrt(half * half - ay * ay);
    const float r = std::min(half, corner * size);
    if (ay <= half - r) return half;
    const float dy = ay - (half - r);
    return half - r + std::sqrt(std::max(0.f, r * r - dy * dy));
}

// Instagram's gradient, yellow at the bottom left to violet at the top right.
Color instagramAt(float u) {
    static const Color stops[] = { mkColor(0xFEDA75), mkColor(0xFA7E1E), mkColor(0xD62976), mkColor(0x962FBF),
                                   mkColor(0x4F5BD5) };
    u = clamp01(u) * 4;
    const int i = std::min(3, (int)u);
    return mixC(stops[i], stops[i + 1], u - i);
}

// The badge shape filled with a diagonal gradient: rows that follow the shape's edges,
// each cut into cells whose corners carry the gradient's colours.
void gradientBadge(const Ctx& c, float size, Shape shape, float corner) {
    const int rows = 48, cols = 24;
    const float half = size / 2;
    auto colorAt = [&](Vector2 p) { return dr::TA(c, instagramAt(((p.x - p.y) / size + 1) / 2)); };
    for (int j = 0; j < rows; j++) {
        const float y0 = -half + size * j / rows, y1 = -half + size * (j + 1) / rows;
        const float e0 = extent(shape, size, corner, y0), e1 = extent(shape, size, corner, y1);
        for (int i = 0; i < cols; i++) {
            const float u0 = (float)i / cols, u1 = (float)(i + 1) / cols;
            const Vector2 p[4] = { { -e0 + 2 * e0 * u0, y0 }, { -e0 + 2 * e0 * u1, y0 }, { -e1 + 2 * e1 * u1, y1 },
                                   { -e1 + 2 * e1 * u0, y1 } };
            Vector2 s[4];
            Color k[4];
            for (int q = 0; q < 4; q++) { s[q] = mapP(c, p[q]); k[q] = colorAt(p[q]); }
            DrawTriangleGradient(s[0], s[2], s[1], k[0], k[2], k[1]);
            DrawTriangleGradient(s[0], s[3], s[2], k[0], k[3], k[2]);
        }
    }
}

vg::Path shapePath(Shape shape, float size, float corner) {
    const float h = size / 2;
    if (shape == Shape::Circle) return vg::Path::circle({ 0, 0 }, h);
    return vg::Path::rect({ -h, -h, size, size }, shape == Shape::Rounded ? corner * size : 0);
}

void solidBadge(const Ctx& c, float size, Shape shape, float corner, Color col) {
    const float h = size / 2;
    if (shape == Shape::Circle) dr::circle(c, { 0, 0 }, h, col);
    else if (shape == Shape::Square) dr::rect(c, -h, -h, size, size, col);
    else dr::roundRect(c, -h, -h, size, size, corner * size, col);
}

} // namespace

const std::vector<std::string>& brands() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (const Brand& b : BRANDS) v.push_back(b.name);
        return v;
    }();
    return names;
}

const std::vector<std::string>& languages() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (const Brand& b : LANGUAGES) v.push_back(b.name);
        return v;
    }();
    return names;
}

const vg::Path& glyph(const std::string& brand) { return prepared(brand).glyph; }
Color brandColor(const std::string& brand) { return mkColor(prepared(brand).brand->color); }
std::string title(const std::string& brand) { return prepared(brand).brand->title; }

Style parseStyle(const std::string& s) {
    if (s == "badge") return Style::Badge;
    if (s == "glyph") return Style::Glyph;
    if (s == "mono") return Style::Mono;
    if (s == "outline") return Style::Outline;
    throw std::runtime_error("icons: unknown style '" + s + "' (badge, glyph, mono or outline)");
}

Shape parseShape(const std::string& s) {
    if (s == "rounded") return Shape::Rounded;
    if (s == "circle") return Shape::Circle;
    if (s == "square") return Shape::Square;
    throw std::runtime_error("icons: unknown shape '" + s + "' (rounded, circle or square)");
}

void draw(const Ctx& c, const std::string& name, Vector2 centre, float size, const Look& look, float reveal) {
    const Prepared& p = prepared(name);
    const Brand& b = *p.brand;
    if (size <= 0 || reveal <= 0) return;
    // Drawing on: the badge grows over the first half, the mark writes itself from 0.2.
    const float grow = reveal >= 1 ? 1 : smooth(reveal * 2), u = reveal >= 1 ? 1 : clamp01((reveal - 0.2f) / 0.8f);
    const bool tik = std::string(b.name) == "tiktok";
    const Ctx box = at(c, centre, 1);
    if (look.style == Style::Glyph || look.style == Style::Mono) {
        const Ctx g = at(c, centre, size / 24);
        if (look.style == Style::Mono) { mark(g, p.glyph, look.color, u); return; }
        const Color col = b.dark ? look.color : mkColor(b.color);
        if (!p.under.empty()) mark(g, p.under, WHITE, u);
        if (tik) tiktok(g, p.glyph, col, u);
        else mark(g, p.glyph, col, u);
        return;
    }
    const float s = size * grow, inner = look.shape == Shape::Circle ? 0.92f : 1.f;
    if (look.style == Style::Outline) {
        vg::stroke(box, shapePath(look.shape, s * 0.96f, look.corner), vg::Stroke(look.color, size * 0.04f));
        mark(at(c, centre, size * 0.52f * inner / 24), p.glyph, look.color, u);
        return;
    }
    if (std::string(b.name) == "instagram") gradientBadge(box, s, look.shape, look.corner);
    else solidBadge(box, s, look.shape, look.corner, mkColor(b.color));
    const Ctx g = at(c, centre, size * b.badgeSize * inner / 24);
    const Color badgeInk = name == "javascript" || name == "c" ? mkColor(0x18202b) : look.color;
    if (tik) tiktok(g, p.badgeMark, badgeInk, u);
    else mark(g, p.badgeMark, badgeInk, u);
}

void logo(const Ctx& c, Vector2 centre, float size, double t, float reveal, bool badge, float spin) {
    if (size <= 0 || reveal <= 0) return;
    const Ctx k = at(c, centre, size / 400);       // the SVG's 400 x 400 units
    const Color teal = mkColor(0x47dad8), iris = mkColor(0x9b8cff), amber = mkColor(0xffc365), ink = mkColor(0xf2f4ff);
    if (badge) {
        const float g = reveal >= 1 ? 1 : smooth(reveal * 2.5f);
        dr::roundRect(k, -200 * g, -200 * g, 400 * g, 400 * g, 72 * g, mkColor(0x0f1528));
    }
    // The ring sweeps on clockwise from the top, its gradient running top left to bottom right.
    const float sweep = 360 * (reveal >= 1 ? 1 : smooth(reveal / 0.6f));
    const int segments = 72;
    for (int i = 0; i < segments; i++) {
        const float a0 = -90 + 360.f * i / segments, a1 = -90 + 360.f * (i + 1) / segments;
        if (a0 >= -90 + sweep) break;
        const float mid = (a0 + a1) / 2 * (float)PI / 180;
        const float u = ((std::cos(mid) + std::sin(mid)) / std::sqrt(2.f) + 1) / 2;
        dr::arc(k, { 0, 0 }, 118, 26, a0, std::min(a1, -90 + sweep), mixC(teal, iris, u));
    }
    // The play triangle springs in.
    const float pop = reveal >= 1 ? 1 : reveal < 0.35f ? 0 : (float)spring((reveal - 0.35f) * 60, SpringOptions{});
    if (pop > 0) {
        const Vector2 tri[3] = { { -32 * pop, -54 * pop }, { 62 * pop, 0 }, { -32 * pop, 54 * pop } };
        dr::fillPoly(k, tri, 3, ink);
    }
    // The play-head arc fades in and orbits.
    const float arcA = reveal >= 1 ? 1 : clamp01((reveal - 0.5f) / 0.3f);
    if (arcA > 0) {
        const float a = -90 + (float)std::fmod(t * spin, 360.0);
        dr::arc(k, { 0, 0 }, 148, 14, a, a + 70 * arcA, withAlpha(amber, arcA));
        dr::circle(k, polar(a + 70 * arcA, 148), 7, withAlpha(amber, arcA));
        dr::circle(k, polar(a, 148), 7, withAlpha(amber, arcA));
    }
}

// ------------------------------------------------------------------ prefabs

namespace {

// How an instance enters, at local time t (frames after its delay).
struct Phase {
    float scale = 1, alpha = 1, reveal = 1;
};

void checkEntrance(const std::string& kind) {
    if (kind != "pop" && kind != "draw" && kind != "fade" && kind != "none")
        throw std::runtime_error("icons: unknown entrance '" + kind + "' (pop, draw, fade or none)");
}

Phase entrance(const std::string& kind, double t, double length, int fps) {
    Phase ph;
    if (kind == "none") return ph;
    if (t < 0) return { 0, 0, 0 };
    if (kind == "pop") {
        SpringOptions o;
        o.fps = fps;
        o.config.damping = 12;
        ph.scale = (float)spring(t, o);
        ph.alpha = clamp01((float)t / 6);
    } else if (kind == "fade") {
        ph.alpha = smooth((float)(t / 12));
    } else {
        ph.reveal = clamp01((float)(t / length));
    }
    return ph;
}

// Something that follows the entrance: 0 until `after` frames in, then fades in.
float follow(const std::string& kind, double t, double after) {
    return kind == "none" ? 1.f : smooth((float)((t - after) / 14));
}

Look lookFrom(const Json& p) {
    Look look;
    look.style = parseStyle(p.str("style", "badge"));
    look.shape = parseShape(p.str("shape", "rounded"));
    look.color = p.color("color", WHITE);
    look.corner = (float)p.num("corner", 0.22);
    return look;
}

} // namespace

void registerPrefabs() {
    // One prefab per brand: the icon, optionally followed by a handle to its right; the
    // pair is centred on the node's origin.
    auto addBadge = [](const std::string& brand, const std::string& id) {
        prefab::add({
            id,
            Json::parse(R"({"size": 160, "style": "badge", "shape": "rounded", "color": "#ffffff", "handle": "",
                            "ink": "#f2f4ff", "in": "pop", "delay": 0, "fps": 30})"),
            [brand](TNode& n, const Json& p) {
                const float size = (float)p.num("size", 160);
                const Look look = lookFrom(p);
                const std::string handle = p.str("handle", ""), in = p.str("in", "pop");
                checkEntrance(in);
                const Color ink = p.color("ink", mkColor(0xf2f4ff));
                const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
                n.fn = [=](const Ctx& c, int frame) {
                    const double t = frameTime(frame) - delay;
                    const Phase ph = entrance(in, t, 30, fps);
                    if (ph.alpha <= 0) return;
                    Ctx k = c;
                    k.a *= ph.alpha;
                    Font font = fonts::ui(true);
                    const float fs = size * 0.36f, gap = size * 0.22f;
                    const float textW = handle.empty() ? 0 : dr::measure(font, handle.c_str(), fs, 0).x;
                    const float x0 = -(size + (handle.empty() ? 0 : gap + textW)) / 2;
                    draw(k, brand, { x0 + size / 2, 0 }, size * ph.scale, look, ph.reveal);
                    if (handle.empty()) return;
                    const float ha = follow(in, t, in == "draw" ? 18 : 8);
                    if (ha > 0)
                        dr::text(font, k, handle.c_str(), { x0 + size + gap - 24 * (1 - ha), 0 }, fs, 0, withAlpha(ink, ha),
                                 0, 0.5);
                };
            },
        });
    };
    for (const std::string& brand : brands()) addBadge(brand, "social-" + brand);
    for (const std::string& language : languages()) addBadge(language, "language-" + language);

    // A row of icons popping in one after another, with an optional handle beneath.
    prefab::add({
        "social-row",
        Json::parse(R"({"brands": ["x", "youtube", "github", "instagram", "tiktok", "whatsapp", "telegram", "twitch"],
                        "size": 120, "gap": 40, "style": "badge", "shape": "rounded", "color": "#ffffff",
                        "stagger": 4, "handle": "", "ink": "#f2f4ff", "in": "pop", "delay": 0, "fps": 30})"),
        [](TNode& n, const Json& p) {
            std::vector<std::string> list;
            for (const Json& b : p["brands"].asArray()) {
                list.push_back(b.asString());
                prepared(list.back());                     // unknown brands fail at build time
            }
            const float size = (float)p.num("size", 120), gap = (float)p.num("gap", 40);
            const Look look = lookFrom(p);
            const std::string handle = p.str("handle", ""), in = p.str("in", "pop");
            checkEntrance(in);
            const Color ink = p.color("ink", mkColor(0xf2f4ff));
            const double stagger = p.num("stagger", 4);
            const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = frameTime(frame) - delay;
                const int count = (int)list.size();
                const float x0 = -(count * size + (count - 1) * gap) / 2 + size / 2;
                for (int i = 0; i < count; i++) {
                    const Phase ph = entrance(in, t - i * stagger, 30, fps);
                    if (ph.alpha <= 0) continue;
                    Ctx k = c;
                    k.a *= ph.alpha;
                    draw(k, list[i], { x0 + i * (size + gap), 0 }, size * ph.scale, look, ph.reveal);
                }
                if (handle.empty()) return;
                const float ha = follow(in, t, (count - 1) * stagger + (in == "draw" ? 24 : 10));
                if (ha > 0)
                    dr::text(fonts::ui(true), c, handle.c_str(), { 0, size / 2 + size * 0.5f + 16 * (1 - ha) }, size * 0.32f,
                             0, withAlpha(ink, ha), 0.5, 0.5);
            };
        },
    });

    // The raymotion logo: the mark with the wordmark beside it (or beneath, "stacked") and
    // an optional tagline, centred on the node's origin. "draw" sweeps the ring on, springs
    // the triangle in and spells the wordmark out.
    prefab::add({
        "raymotion-logo",
        Json::parse(R"({"size": 220, "wordmark": true, "tagline": "", "layout": "horizontal", "badge": false,
                        "spin": 2, "ink": "#f2f4ff", "accent": "#47dad8", "in": "draw", "length": 40, "delay": 0,
                        "fps": 30})"),
        [](TNode& n, const Json& p) {
            const float size = (float)p.num("size", 220), spin = (float)p.num("spin", 2);
            const bool wordmark = p.flag("wordmark", true), badge = p.flag("badge", false);
            const std::string tagline = p.str("tagline", ""), layout = p.str("layout", "horizontal"),
                              in = p.str("in", "draw");
            checkEntrance(in);
            if (layout != "horizontal" && layout != "stacked")
                throw std::runtime_error("raymotion-logo: layout is horizontal or stacked, not '" + layout + "'");
            const Color ink = p.color("ink", mkColor(0xf2f4ff)), accent = p.color("accent", mkColor(0x47dad8));
            const double length = std::max(1.0, p.num("length", 40));
            const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = frameTime(frame) - delay;
                const Phase ph = entrance(in, t, length * 0.7, fps);
                if (ph.alpha <= 0) return;
                Ctx k = c;
                k.a *= ph.alpha;
                const float mark = size * (badge ? 1.f : 0.8f);    // the mark's visible width
                Font bold = fonts::ui(true), light = fonts::ui(false);
                const float fs = size * 0.42f, ts = fs * 0.36f, gap = size * 0.14f;
                const char* word = "raymotion";
                const float wordW = wordmark ? dr::measure(bold, word, fs, 0).x : 0;
                const float tagW = tagline.empty() ? 0 : dr::measure(light, tagline.c_str(), ts, 2).x;
                const bool stacked = layout == "stacked";
                Vector2 centre{ 0, 0 }, wordAt{ 0, 0 }, tagAt{ 0, 0 };
                if (!wordmark) {
                    tagAt = { 0, mark / 2 + gap + ts / 2 };
                } else if (stacked) {
                    const float h = mark + gap + fs + (tagline.empty() ? 0 : ts * 1.6f);
                    centre = { 0, -h / 2 + mark / 2 };
                    wordAt = { 0, -h / 2 + mark + gap + fs / 2 };
                    tagAt = { 0, wordAt.y + fs / 2 + ts * 0.9f };
                } else {
                    const float w = mark + gap + std::max(wordW, tagW);
                    centre = { -w / 2 + mark / 2, 0 };
                    wordAt = { -w / 2 + mark + gap, tagline.empty() ? 0 : -ts * 0.7f };
                    tagAt = { wordAt.x, fs * 0.42f };
                }
                logo(k, centre, size * ph.scale, t, ph.reveal, badge, spin);
                if (wordmark) {
                    // Letters rise into place one after another.
                    const float u = in == "draw" ? smooth((float)((t - length * 0.3) / (length * 0.6))) : 1.f;
                    const int letters = 9;
                    for (int i = 0; i < letters; i++) {
                        const float a = clamp01((u * (letters + 3) - i) / 3);
                        if (a <= 0) break;
                        const std::string before(word, i), one(word + i, 1);
                        const float dx = before.empty() ? 0 : dr::measure(bold, before.c_str(), fs, 0).x;
                        const float x = stacked ? wordAt.x - wordW / 2 + dx : wordAt.x + dx;
                        dr::text(bold, k, one.c_str(), { x, wordAt.y + fs * 0.25f * (1 - a) }, fs, 0, withAlpha(ink, a), 0,
                                 0.5);
                    }
                }
                if (!tagline.empty()) {
                    const float ta = follow(in, t, in == "draw" ? length * 0.8 : 6);
                    if (ta > 0)
                        dr::text(light, k, tagline.c_str(), tagAt, ts, 2, withAlpha(accent, ta),
                                 stacked || !wordmark ? 0.5 : 0, 0.5);
                }
            };
        },
    });
}

} // namespace icons
} // namespace rmtn
