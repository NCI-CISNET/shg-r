\name{SmokingHistoryGenerator-package}
\alias{SmokingHistoryGenerator-package}
\alias{SmokingHistoryGenerator}
\docType{package}
\title{
\packageTitle{SmokingHistoryGenerator}
}
\description{
\packageDescription{SmokingHistoryGenerator}
}
\details{
Default calibrated inputs are installed as flat CSV files under \code{system.file("extdata", package = "SmokingHistoryGenerator")}: \code{initiation.csv}, \code{cessation.csv}, \code{cpd.csv}, and (for mortality) either \code{acm.csv} or \code{ocm-excl-lung-cancer.csv}; set \code{mortality_filename} accordingly. Wide legacy \code{.txt} tables remain supported. A minimal subset is bundled for CRAN; the full NHIS-style tables are distributed separately (Zenodo; the installed README is at \code{system.file("README.md", package = "SmokingHistoryGenerator")}).
}
\author{
\packageAuthor{SmokingHistoryGenerator}

Maintainer: \packageMaintainer{SmokingHistoryGenerator}
}
\references{
CISNET modelling applications that use the Smoking History Generator are listed
on the CISNET website at \url{https://cisnet.cancer.gov/}.
}
\keyword{package}

%% Uncomment below to imitate parts of library(help = SmokingHistoryGenerator)
%\section{The \file{DESCRIPTION} File}{\packageDESCRIPTION{SmokingHistoryGenerator}}
%\section{Documentation Index}{\packageIndices{SmokingHistoryGenerator}}

\seealso{
R package source: \url{https://github.com/NCI-CISNET/shg-r}
}
