######################################################################
# Define vector of WTP thresholds
v.wtp <- seq(0, 120000, by = 2500)
# Matrix with effectiveness for each strategy as columns
m.e
# Matrix with costs for each strategy as columns
m.c
# Number of simulations
n.sim <- nrow(m.e)
# Number of strategies
n.str <- ncol(m.e)
# Matrix to store proportion of each strategy is cost-effective across all WTP thresholds
m.ceac <- matrix(0, nrow = length(v.wtp), ncol = n.str)
# Vector to store strategy with highest expected net benefit across all WTP thresholds
v.ceaf <- matrix(0, nrow = length(v.wtp), ncol = 1)
# Matrix to store expected losses
m.exp.loss <- matrix(0, nrow = length(v.wtp), ncol = n.str)
# Compute expected losses for all strategies across all WTP thresholds
for(l in 1:length(v.wtp)) { # l <- 10
  # Matrix of NMB: Effectiveness minus Costs, with vector indexing
  m.nmb <- m.e * v.wtp[l] - m.c
  ## CEAC
  # Obtain strategy with maximum NMB at each iteration
  max.str <- max.col(m.nmb)
  # Obtain number of times each strategy is cost-effective
  n.ce <- table(max.str)
  # Obtain proportion that each strategy is cost-effective
  m.ceac[l, as.numeric(names(n.ce))] <- n.ce/n.sim
  ## CEAF
  v.ceaf[l, 1] <- which.max(colMeans(m.nmb))
  ## ELC
  # Compute losses for each strategy at each iteration
  m.loss <- m.nmb[cbind(1:n.sim, max.str)] - m.nmb
  # Calculate expected loss for each strategy
  m.exp.loss[l, ] <- colMeans(m.loss)
}
# Optimal strategy based on lowest expected loss
v.optimal.str <- max.col(-m.exp.loss) # implements the argmax function
# Expected loss of optimal strategy (i.e., EVPI)
v.optimal.el <- m.exp.loss[cbind(1:length(v.wtp), v.optimal.str)]
######################################################################
