-- HeidiSQL: Run on CHARACTERS database
-- Backfill profession headers for existing characters so they persist through relog.

INSERT INTO `character_spell` (`guid`, `spell`, `active`, `disabled`)
SELECT cs.`guid`, m.`header_spell`, 1, 0
FROM `character_skills` cs
JOIN (
    SELECT 2259 AS skill_line, 195095 AS header_spell
    UNION ALL SELECT 3863 AS skill_line, 195095 AS header_spell
    UNION ALL SELECT 2018 AS skill_line, 195097 AS header_spell
    UNION ALL SELECT 3860 AS skill_line, 195097 AS header_spell
    UNION ALL SELECT 2550 AS skill_line, 195128 AS header_spell
    UNION ALL SELECT 3861 AS skill_line, 195128 AS header_spell
    UNION ALL SELECT 7411 AS skill_line, 195096 AS header_spell
    UNION ALL SELECT 3865 AS skill_line, 195096 AS header_spell
    UNION ALL SELECT 4036 AS skill_line, 195112 AS header_spell
    UNION ALL SELECT 3866 AS skill_line, 195112 AS header_spell
    UNION ALL SELECT 356 AS skill_line, 158743 AS header_spell
    UNION ALL SELECT 3862 AS skill_line, 158743 AS header_spell
    UNION ALL SELECT 45357 AS skill_line, 195115 AS header_spell
    UNION ALL SELECT 3867 AS skill_line, 195115 AS header_spell
    UNION ALL SELECT 25229 AS skill_line, 195116 AS header_spell
    UNION ALL SELECT 3864 AS skill_line, 195116 AS header_spell
    UNION ALL SELECT 2108 AS skill_line, 195119 AS header_spell
    UNION ALL SELECT 3868 AS skill_line, 195119 AS header_spell
    UNION ALL SELECT 3908 AS skill_line, 195126 AS header_spell
    UNION ALL SELECT 3869 AS skill_line, 195126 AS header_spell
    UNION ALL SELECT 182 AS skill_line, 195114 AS header_spell
    UNION ALL SELECT 2832 AS skill_line, 195114 AS header_spell
    UNION ALL SELECT 186 AS skill_line, 195122 AS header_spell
    UNION ALL SELECT 2833 AS skill_line, 195122 AS header_spell
    UNION ALL SELECT 393 AS skill_line, 195125 AS header_spell
    UNION ALL SELECT 2834 AS skill_line, 195125 AS header_spell
) m ON m.skill_line = cs.`skill`
LEFT JOIN `character_spell` sp ON sp.`guid` = cs.`guid` AND sp.`spell` = m.`header_spell`
WHERE cs.`value` > 0 AND sp.`guid` IS NULL;

-- Also backfill headers from already-known profession spells (classic/DF/TWW learn spell IDs).
INSERT INTO `character_spell` (`guid`, `spell`, `active`, `disabled`)
SELECT DISTINCT known.`guid`, m.`header_spell`, 1, 0
FROM `character_spell` known
JOIN (
    SELECT 264211 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264212 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264213 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264214 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264220 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264221 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264243 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264244 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264245 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264246 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264247 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264248 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264250 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264251 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264255 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264256 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 265787 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 265788 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 309822 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 309823 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 366248 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 366261 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 423322 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 423321 AS source_spell, 195095 AS header_spell
    UNION ALL SELECT 264434 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264435 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264436 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264437 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264438 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264439 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264440 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264441 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264442 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264443 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264444 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264445 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264446 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264447 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264448 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 264449 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 265803 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 265804 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 309827 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 309828 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 365677 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 365699 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 423332 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 423344 AS source_spell, 195097 AS header_spell
    UNION ALL SELECT 818 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264632 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264633 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264634 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264635 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264636 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264637 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264638 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264639 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264640 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264641 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264642 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264643 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264644 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264645 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264646 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264647 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 265817 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 265818 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 309830 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 309831 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 366256 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 366246 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 423333 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 423345 AS source_spell, 195128 AS header_spell
    UNION ALL SELECT 264455 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264457 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264460 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264461 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264462 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264463 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264464 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264466 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264467 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264468 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264469 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264470 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264471 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264472 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264473 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264474 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 265805 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 265806 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 309832 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 309833 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 366255 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 366245 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 423334 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 423346 AS source_spell, 195096 AS header_spell
    UNION ALL SELECT 264475 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264478 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264479 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264480 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264481 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264482 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264483 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264484 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264485 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264486 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264487 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264488 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264490 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264491 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264492 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 264493 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 265807 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 265808 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 310539 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 310542 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 366244 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 366254 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 423335 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 423347 AS source_spell, 195112 AS header_spell
    UNION ALL SELECT 7620 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 7733 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 110412 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 158744 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271617 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271657 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271659 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271661 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271663 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271665 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271673 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271719 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 271720 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 310676 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 366243 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 423348 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 471021 AS source_spell, 158743 AS header_spell
    UNION ALL SELECT 264494 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264495 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264496 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264497 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264498 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264499 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264500 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264501 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264502 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264503 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264504 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264505 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264506 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264507 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264508 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264509 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 265809 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 265810 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 309805 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 309804 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 366251 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 366241 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 423338 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 423350 AS source_spell, 195115 AS header_spell
    UNION ALL SELECT 264532 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264533 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264534 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264535 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264537 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264538 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264539 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264541 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264542 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264543 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264544 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264545 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264546 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264547 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264548 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264549 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 265811 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 265812 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 311967 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 311969 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 366250 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 366240 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 423339 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 423351 AS source_spell, 195116 AS header_spell
    UNION ALL SELECT 264577 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264578 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264579 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264580 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264581 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264582 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264583 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264584 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264585 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264586 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264588 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264589 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264590 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264591 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264592 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264593 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 265813 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 265814 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 309038 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 309039 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 366239 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 366249 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 423340 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 423352 AS source_spell, 195119 AS header_spell
    UNION ALL SELECT 264616 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264617 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264618 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264619 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264620 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264621 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264622 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264623 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264624 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264625 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264626 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264627 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264628 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264629 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264630 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 264631 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 265815 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 265816 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 310949 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 310950 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 366258 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 366262 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 423343 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 423355 AS source_spell, 195126 AS header_spell
    UNION ALL SELECT 423349 AS source_spell, 195114 AS header_spell
    UNION ALL SELECT 423353 AS source_spell, 195122 AS header_spell
    UNION ALL SELECT 423354 AS source_spell, 195125 AS header_spell
) m ON m.source_spell = known.`spell`
LEFT JOIN `character_spell` sp ON sp.`guid` = known.`guid` AND sp.`spell` = m.`header_spell`
WHERE sp.`guid` IS NULL;

-- Optional gather-header aliases (safe to re-run).
INSERT INTO `character_spell` (`guid`, `spell`, `active`, `disabled`)
SELECT sp.`guid`, a.`alias_spell`, 1, 0
FROM `character_spell` sp
JOIN (
    SELECT 195114 AS base_spell, 265820 AS alias_spell
    UNION ALL SELECT 265820 AS base_spell, 195114 AS alias_spell
    UNION ALL SELECT 195122 AS base_spell, 265838 AS alias_spell
    UNION ALL SELECT 265838 AS base_spell, 195122 AS alias_spell
    UNION ALL SELECT 195125 AS base_spell, 265856 AS alias_spell
    UNION ALL SELECT 265856 AS base_spell, 195125 AS alias_spell
) a ON a.base_spell = sp.`spell`
LEFT JOIN `character_spell` e ON e.`guid` = sp.`guid` AND e.`spell` = a.`alias_spell`
WHERE e.`guid` IS NULL;
