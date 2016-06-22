import numpy as np

from baxcat.dist import nng
from baxcat.dist import csd
from baxcat.misc import pflip

from scipy.misc import logsumexp
from math import log


def _get_view_weights(model, col_idx):
    view_idx = int(model['col_assignment'][col_idx])
    alpha = model['view_alphas'][view_idx]
    weights = np.array(model['view_counts'][view_idx] + [alpha])

    return weights/np.sum(weights)


# private sample utils
# `````````````````````````````````````````````````````````````````````````````
def _sample_component_continuous(model, col_idx, component_idx):
    hypers = model['col_hypers'][col_idx]
    if component_idx < len(model['col_suffstats'][col_idx]):
        suffstats = model['col_suffstats'][col_idx][component_idx]
    else:
        suffstats = {'n': 0., 'sum_x': 0., 'sum_x_sq': 0.}

    return nng.sample(suffstats, hypers)


def _sample_component_categorical(model, col_idx, component_idx):
    hypers = model['col_hypers'][col_idx]
    if component_idx < len(model['col_suffstats'][col_idx]):
        suffstats = model['col_suffstats'][col_idx][component_idx]
    else:
        k = len(model['col_suffstats'][0]['counts'])
        suffstats = {'n': 0., 'counts': [0.]*k}

    return csd.sample(suffstats, hypers)


DRAWFUNC = {
    b'continuous': _sample_component_continuous,
    b'categorical': _sample_component_continuous}


def _sample_single_col(model, col_idx, n=1):
    """ Samples data from the column at col_idx """
    weights = _get_view_weights(model, col_idx)
    component_idx = pflip(weights)
    f = DRAWFUNC[model['dtypes'][col_idx]]
    x = f(model, col_idx, component_idx)

    return x


def _sample_multi_col(model, col_idxs, n=1):
    n_cols = len(col_idxs)
    assert n_cols > 1

    view_idxs = [model['col_assignment'][col_idx] for col_idx in col_idxs]

    col2pos = dict((col_idx, i) for i, col_idx in enumerate(col_idxs))

    view2col = dict()
    for col_idx, view_idx in zip(col_idxs, view_idxs):
        view2col[view_idx] = view2col.get(view_idx, []) + [col_idx]

    samples = np.zeros((n, n_cols,))
    for i in range(n):
        for view, cols in view2col.items():
            weights = _get_view_weights(model, cols[0])
            component_idx = pflip(weights)
            for col_idx in cols:
                f = DRAWFUNC[model['dtypes'][col_idx]]
                x = f(model, col_idx, component_idx)
                samples[i, col2pos[col_idx]] = x

    # import pdb; pdb.set_trace()
    if n > 1:
        return samples
    else:
        return samples[0, :]


# private probability utils
# `````````````````````````````````````````````````````````````````````````````
def _probability_component_continuous(x, model, col_idx, component_idx):
    hypers = model['col_hypers'][col_idx]
    if component_idx < len(model['col_suffstats'][col_idx]):
        suffstats = model['col_suffstats'][col_idx][component_idx]
    else:
        suffstats = {'n': 0., 'sum_x': 0., 'sum_x_sq': 0.}

    return nng.probability(x, suffstats, hypers)


def _probability_component_categorical(x, model, col_idx, component_idx):
    hypers = model['col_hypers'][col_idx]
    if component_idx < len(model['col_suffstats'][col_idx]):
        suffstats = model['col_suffstats'][col_idx][component_idx]
    else:
        k = len(model['col_suffstats'][0]['counts'])
        suffstats = {'n': 0., 'counts': [0.]*k}

    return csd.probability(x, suffstats, hypers)


PROBFUNC = {
    b'continuous': _probability_component_continuous,
    b'categorical': _probability_component_continuous}


def _probability_single_col(x, model, col_idx):
    """ probability of x from col_idx under model  """
    log_weights = np.log(_get_view_weights(model, col_idx))
    logps = np.zeros(len(log_weights))
    for component_idx, log_weight in enumerate(log_weights):
        f = PROBFUNC[model['dtypes'][col_idx]]
        logps[component_idx] = f(x, model, col_idx, component_idx) + log_weight

    return logsumexp(logps)


def _probability_multi_col(x, model, col_idxs):
    view_idxs = [model['col_assignment'][col_idx] for col_idx in col_idxs]

    col2pos = dict((col_idx, i) for i, col_idx in enumerate(col_idxs))

    view2col = dict()
    for col_idx, view_idx in zip(col_idxs, view_idxs):
        view2col[view_idx] = view2col.get(view_idx, []) + [col_idx]

    view_idxs = list(set(view_idxs))
    clstr_cts = np.ones(len(view_idxs), dtype=int)  # +1 for unonserved cluster
    for i, view_idx in enumerate(view_idxs):
        col_idx = view2col[view_idx][0]
        clstr_cts[i] += len(model['col_suffstats'][col_idx])

    logp = 0.
    for v, view_idx in enumerate(view_idxs):
        cols = view2col[view_idx]
        log_weights = np.log(_get_view_weights(model, cols[0]))
        lp_view = np.copy(log_weights)
        for col_idx in cols:
            y = x[col2pos[col_idx]]
            f = PROBFUNC[model['dtypes'][col_idx]]
            for k, log_weight in enumerate(log_weights):
                lp_view[k] += f(y, model, col_idx, k)
        logp += logsumexp(lp_view)

    return logp


# Main interface
# `````````````````````````````````````````````````````````````````````````````
def sample(models, col_idxs, n=1):
    """ Sample from the model(s) likelihood.

    Parameters
    ----------
    models : list(dict)
        List of metadata from and Engine instance (the `models` member).
    col_idxs : list(int)
        List of integer column indices (not columns names).
    n : int
        The number of samples

    Returns
    -------
    x : numpy.ndarray(float)
        Unconverted data array. each row is a sample.
    """
    # FIXME: docstring
    n_cols = len(col_idxs)
    n_models = len(models)

    # predetermine from which models to sample
    midxs = np.random.randint(n_models, size=n)

    samples = np.zeros((n, n_cols,))
    for i, midx in enumerate(midxs):
        if n_cols == 1:
            x = _sample_single_col(models[midx], col_idxs[0])
            samples[i] = x
        else:
            x = _sample_multi_col(models[midx], col_idxs)
            samples[i, :] = x

    return samples


def probability(x, models, col_idxs):
    """ The average probability of x under the models

    Parameters
    ----------
    x : array-like(d,)
        A d-length array where each entry comes from a distrinct feature
    models : list(dict)
        List of metadata from and Engine instance (the `_models` member).
    col_idxs : list(int)
        List of integer column indices (not columns names).

    Returns
    -------
    p : float
        The log probability.

    Example
    -------
    >>> x = [1., 3., 21.1]
    >>> col_idxs = [3, 12, 16]
    >>> model = engine.models
    >>> lp = probability(x, models, col_idxs)
    """

    assert isinstance(col_idxs, (list, tuple,))
    # FIXME: docstring
    n_cols = len(col_idxs)
    n_models = len(models)

    logps = np.zeros(max(x.shape))
    for i, x_i in enumerate(x):
        logps_m = np.zeros(n_models)
        for j, model in enumerate(models):
            if n_cols == 1:
                lp = _probability_single_col(x_i, model, col_idxs[0])
            else:
                lp = _probability_multi_col(x_i, model, col_idxs)

            logps_m[j] = lp

        logps[i] = logsumexp(logps_m) - log(n_models)

    return np.sum(logps)


def joint_entropy(models, col_idxs, n_samples=1000):
    """ The average joint entropy of the columns under the models.

    Parameters
    ----------
    models : list(dict)
        List of metadata from and Engine instance (the `_models` member).
    col_idxs : list(int)
        List of integer column indices (not columns names).
    n_samples : int
        The number of samples to use for the Monte Carlo approximation.

    Returns
    -------
    h : float
        An estimate of the joint entropy.
    """
    n_models = len(models)

    # predetermine from which models to sample
    midxs = np.random.randint(n_models, size=n_samples)
    ps = np.zeros(n_samples)
    for i, midx in enumerate(midxs):
        x = sample([models[midx]], col_idxs, n=n_samples)
        ps[i] = probability(x, [models[midx]], col_idxs)

    return -np.sum(ps) / n_samples
