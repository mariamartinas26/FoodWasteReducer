CREATE TABLE beneficiari (
    nume TEXT NOT NULL UNIQUE,
    parola TEXT NOT NULL
);
CREATE TABLE donatori (
    nume TEXT NOT NULL UNIQUE,
    parola TEXT NOT NULL
);
CREATE TABLE anunturi (
    nume TEXT NOT NULL,
    produs TEXT NOT NULL,
    cantitate INTEGER NOT NULL,
    data_expirare DATE NOT NULL,
    data_creare DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE rezervari (
    nume TEXT NOT NULL,
    produs TEXT NOT NULL,
    cantitate INTEGER NOT NULL
);