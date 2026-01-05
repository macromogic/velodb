SELECT
    l_extendedprice
FROM
    lineitem JOIN part ON p_partkey = l_partkey
WHERE
    p_brand = 'Brand#23'
    AND p_container = 'MED BOX';