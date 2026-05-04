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
    \enumerate{
        \item \href{https://resources.cisnet.cancer.gov/projects/\#shg/tcpd}{Tobacco Control and the Reduction in Smoking-Related Premature Deaths in the United States, 1964-2012}
        \item \href{https://resources.cisnet.cancer.gov/projects/\#shg/tce}{Patterns of Birth Cohort-Specific Smoking Histories, 1965-2009}
        \item \href{https://resources.cisnet.cancer.gov/projects/\#shg/iomr}{Public Health Implications of Raising the Minimum Age of Legal Access to Tobacco Products}
        \item \href{https://resources.cisnet.cancer.gov/projects/\#shg/sbc2}{Smoking and Lung Cancer Mortality in the United States From 2015 to 2065: A Comparative Modeling Approach}
    }
}
\keyword{package}

%% Uncomment below to imitate parts of library(help = SmokingHistoryGenerator)
%\section{The \file{DESCRIPTION} File}{\packageDESCRIPTION{SmokingHistoryGenerator}}
%\section{Documentation Index}{\packageIndices{SmokingHistoryGenerator}}

\seealso{
CISNET Smoking History Generator source code: \url{https://github.com/CSNW/smoking-history-generator/}
}
